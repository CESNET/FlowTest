/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief DNS layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dns.h"
#include "../domainnamegenerator.h"
#include "../flowplanhelper.h"
#include "../randomgenerator.h"
#include "../utils.h"

#include <pcapplusplus/DnsLayer.h>
#include <pcapplusplus/PayloadLayer.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <endian.h>

namespace generator {

static constexpr int DNS_MAX_PKT_SIZE = 512;
static constexpr int EDNS_MAX_PKT_SIZE = 4096;

static constexpr int DNS_MAX_FWD_PKT_SIZE = DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN
	+ DNS_MAX_DOMAIN_SIZE + DNS_OPT_RR_REQUIRED_BYTES;
static constexpr int DNS_MIN_FWD_PKT_SIZE = DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN
	+ DNS_MIN_DOMAIN_SIZE + DNS_OPT_RR_REQUIRED_BYTES;
// OPT record to be able to signal ability to accept payload size >512 if necessary

static constexpr int DNS_MIN_REV_PKT_SIZE = DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN
	+ DNS_MIN_DOMAIN_SIZE + DNS_A_ANSWER_SIZE_EXCL_DOMAIN;

namespace {

enum DnsParams : int { ResponseSize, OrigPktSize };

} // namespace

void Dns::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);
	Direction dir = Direction::Forward;

	if (planner.PktsRemaining(dir) == 0) {
		dir = Direction::Reverse;
	}

	while (planner.PktsTillEnd() > 0) {
		assert(planner.PktsRemaining(dir) > 0);

		Packet* pkt = planner.NextPacket();
		pkt->_direction = dir;

		// Signals the minimum packet length to the planner
		pkt->_size += dir == Direction::Forward ? DNS_MIN_FWD_PKT_SIZE : DNS_MIN_REV_PKT_SIZE;

		Packet::layerParams params;
		pkt->_layers.push_back({this, params});

		planner.IncludePkt(pkt);

		if (planner.PktsRemaining(SwapDirection(dir)) > 0) {
			dir = SwapDirection(dir);
		}
	}
}

size_t Dns::GetDnsPayloadSize(Packet& plan) const
{
	// WARNING: This only provides valid result when called from Plan/PostPlan phase!
	auto offset = GetPrevLayer()->SizeUpToIpLayer(plan);
	assert(plan._size >= offset);
	return plan._size - offset;
}

void Dns::ProcessPlannedFlow(Flow& flow)
{
	Packet* lastFwdPkt = nullptr;

	for (auto& pkt : flow._packets) {
		if (!PacketHasLayer(pkt)) {
			continue;
		}

		if (pkt._direction == Direction::Forward) {
			if (GetDnsPayloadSize(pkt) > DNS_MAX_FWD_PKT_SIZE) {
				_generateRandomPayloadInsteadOfDns = true;
				return;
			}

			lastFwdPkt = &pkt;

		} else if (pkt._direction == Direction::Reverse) {
			uint64_t responseSize = GetDnsPayloadSize(pkt);

			if (lastFwdPkt) {
				uint64_t querySize = GetDnsPayloadSize(*lastFwdPkt);
				if (responseSize < querySize + DNS_A_ANSWER_SIZE_EXCL_DOMAIN) {
					_generateRandomPayloadInsteadOfDns = true;
					return;
				}
				auto& params = GetPacketParams(*lastFwdPkt);
				params[DnsParams::ResponseSize] = responseSize;
				lastFwdPkt = nullptr;

			} else {
				if (responseSize < DNS_MIN_REV_PKT_SIZE) {
					_generateRandomPayloadInsteadOfDns = true;
					return;
				}
			}
		}
	}
}

void Dns::PostPlanFlow(Flow& flow)
{
	ProcessPlannedFlow(flow);

	if (_generateRandomPayloadInsteadOfDns) {
		ReplanSizesToUniformDistribution(flow);

		_generateRandomPayloadInsteadOfDns = false;
		ProcessPlannedFlow(flow);

		if (_generateRandomPayloadInsteadOfDns) {
			RevertSizesReplan(flow);
		}
	}

	if (_generateRandomPayloadInsteadOfDns) {
		_logger->debug("Generating random payload instead of DNS traffic for flow");
	}
}

void Dns::ReplanSizesToUniformDistribution(Flow& flow)
{
	uint64_t fwdBytes = 0;
	uint64_t fwdPkts = 0;
	uint64_t revBytes = 0;
	uint64_t revPkts = 0;

	for (auto& pkt : flow._packets) {
		if (!PacketHasLayer(pkt)) {
			continue;
		}

		if (pkt._direction == Direction::Forward) {
			fwdBytes += pkt._size;
			fwdPkts++;
		} else if (pkt._direction == Direction::Reverse) {
			revBytes += pkt._size;
			revPkts++;
		}
	}

	uint64_t avgFwdSize = SafeDiv(fwdBytes, fwdPkts);
	uint64_t avgRevSize = SafeDiv(revBytes, revPkts);

	for (auto& pkt : flow._packets) {
		if (!PacketHasLayer(pkt)) {
			continue;
		}

		auto& params = GetPacketParams(pkt);
		params[DnsParams::OrigPktSize] = pkt._size;

		uint64_t minPktSize = GetPrevLayer()->SizeUpToIpLayer(pkt);
		if (pkt._direction == Direction::Forward) {
			minPktSize += DNS_MIN_FWD_PKT_SIZE;
			pkt._size = std::max(minPktSize, avgFwdSize);
		} else if (pkt._direction == Direction::Reverse) {
			minPktSize += DNS_MIN_REV_PKT_SIZE;
			pkt._size = std::max(minPktSize, avgRevSize);
		}
	}
}

void Dns::RevertSizesReplan(Flow& flow)
{
	for (auto& pkt : flow._packets) {
		if (!PacketHasLayer(pkt)) {
			continue;
		}

		auto& params = GetPacketParams(pkt);
		pkt._size = std::get<uint64_t>(params[DnsParams::OrigPktSize]);
	}
}

void Dns::PlanCommunication(int64_t querySize, int64_t responseSize)
{
	assert(querySize != -1 || responseSize != -1);
	assert(querySize == -1 || querySize >= DNS_MIN_FWD_PKT_SIZE);
	assert(responseSize == -1 || responseSize >= DNS_MIN_REV_PKT_SIZE);
	assert(responseSize == -1 || responseSize >= querySize + DNS_A_ANSWER_SIZE_EXCL_DOMAIN);

	_useEdns = false;

	auto& rng = RandomGenerator::GetInstance();

	// Transaction ID
	_transactionId = rng.RandomUInt(1, std::numeric_limits<uint16_t>::max());

	// Domain name
	if (querySize >= 0) {
		int64_t requiredBytes = DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN;
		if (responseSize > DNS_MAX_PKT_SIZE || querySize - requiredBytes > DNS_MAX_DOMAIN_SIZE) {
			requiredBytes += DNS_OPT_RR_REQUIRED_BYTES; // EDNS
			_useEdns = true;
		}
		assert(querySize > requiredBytes);

		_domainName = DomainNameGenerator::GetInstance().Generate(querySize - requiredBytes);

	} else {
		// If there is no query
		int64_t requiredBytes
			= DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN + DNS_A_ANSWER_SIZE_EXCL_DOMAIN;
		assert(responseSize > requiredBytes);

		uint64_t domainSize = std::clamp<uint64_t>(
			rng.RandomDouble() * (responseSize - requiredBytes),
			DNS_MIN_DOMAIN_SIZE,
			DNS_MAX_DOMAIN_SIZE);

		_domainName = DomainNameGenerator::GetInstance().Generate(domainSize);
	}

	// If there is no response
	if (responseSize < 0) {
		_type = rng.RandomChoice(std::array {DnsRRType::A, DnsRRType::AAAA, DnsRRType::CNAME});
		return;
	}

	// The response
	responseSize -= DNS_HEADER_SIZE + DNS_QUESTION_SIZE_EXCL_DOMAIN + _domainName.size();

	int64_t minCnameRecordSizeWithCompression
		= DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN + DNS_MIN_DOMAIN_SIZE;
	int64_t maxCnameRecordSizeWithCompression
		= DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN + DNS_MAX_DOMAIN_SIZE;
	int64_t minCnameRecordSizeWithoutCompression
		= _domainName.size() + DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN + DNS_MIN_DOMAIN_SIZE;
	int64_t maxCnameRecordSizeWithoutCompression
		= _domainName.size() + DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN + DNS_MAX_DOMAIN_SIZE;

	if (responseSize % (DNS_A_ANSWER_SIZE_EXCL_DOMAIN + _domainName.size()) == 0) {
		_numAnswers = responseSize / (DNS_A_ANSWER_SIZE_EXCL_DOMAIN + _domainName.size());
		_useCompression = false;
		_type = DnsRRType::A;

	} else if (responseSize % DNS_A_ANSWER_SIZE_EXCL_DOMAIN == 0) {
		_numAnswers = responseSize / DNS_A_ANSWER_SIZE_EXCL_DOMAIN;
		_useCompression = true;
		_type = DnsRRType::A;

	} else if (responseSize % (DNS_AAAA_ANSWER_SIZE_EXCL_DOMAIN + _domainName.size()) == 0) {
		_numAnswers = responseSize / (DNS_AAAA_ANSWER_SIZE_EXCL_DOMAIN + _domainName.size());
		_useCompression = false;
		_type = DnsRRType::AAAA;

	} else if (responseSize % DNS_AAAA_ANSWER_SIZE_EXCL_DOMAIN == 0) {
		_numAnswers = responseSize / DNS_AAAA_ANSWER_SIZE_EXCL_DOMAIN;
		_useCompression = true;
		_type = DnsRRType::AAAA;

	} else if (
		responseSize >= minCnameRecordSizeWithCompression
		&& responseSize <= maxCnameRecordSizeWithCompression) {
		_numAnswers = 1;
		_type = DnsRRType::CNAME;
		_useCompression = true;

	} else if (
		responseSize >= minCnameRecordSizeWithoutCompression
		&& responseSize <= maxCnameRecordSizeWithoutCompression) {
		_numAnswers = 1;
		_type = DnsRRType::CNAME;
		_useCompression = false;

	} else {
		_type = DnsRRType::TXT;
		_useCompression = true;
		uint64_t minAnswers
			= ceil(double(responseSize) / (DNS_TXT_ANSWER_SIZE_EXCL_DATA + DNS_MAX_TXT_DATA_LEN));
		uint64_t maxAnswers = floor(double(responseSize) / (DNS_TXT_ANSWER_SIZE_EXCL_DATA + 1));
		assert(minAnswers <= maxAnswers);
		_numAnswers = RandomGenerator::GetInstance().RandomUInt(minAnswers, maxAnswers);
	}
}

void Dns::BuildForwardPacket(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	auto responseSize = std::get_if<uint64_t>(&params[DnsParams::ResponseSize]);
	PlanCommunication(plan._size, responseSize ? *responseSize : -1);

	DnsBuilder builder;
	builder.AppendDnsHeader(_transactionId, DNS_FLAGS_RECURSION_DESIRED, 1, 0, 0, _useEdns ? 1 : 0);
	builder.AppendDnsQuestion(_domainName, _type);

	if (_useEdns) {
		builder.AppendDnsOPTRecord(EDNS_MAX_PKT_SIZE, 0);
	}

	auto layer = builder.ToLayer();
	assert(plan._size == layer->getDataLen());
	plan._size = 0;
	packet.addLayer(layer.release(), true);
}

void Dns::BuildReversePacket(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;

	auto& rng = RandomGenerator::GetInstance();

	if (_domainName.empty()) {
		PlanCommunication(-1, plan._size);
	}

	DnsBuilder builder;
	builder.AppendDnsHeader(
		_transactionId,
		DNS_FLAGS_RESPONSE | DNS_FLAGS_RECURSION_DESIRED | DNS_FLAGS_RECURSION_AVAILABLE,
		1,
		_numAnswers);

	uint16_t domainNameOffset = builder.Length();
	builder.AppendDnsQuestion(_domainName, _type);

	// Answers
	std::vector<uint64_t> txtAnswerSizes;
	if (_type == DnsRRType::TXT) {
		txtAnswerSizes = rng.RandomlyDistribute(
			plan._size - builder.Length(),
			_numAnswers,
			DNS_TXT_ANSWER_SIZE_EXCL_DATA + 1,
			DNS_TXT_ANSWER_SIZE_EXCL_DATA + DNS_MAX_TXT_DATA_LEN);
	}

	for (int i = 0; i < _numAnswers; i++) {
		std::variant<const std::string*, uint16_t> rrDomainName;
		if (_useCompression) {
			rrDomainName = domainNameOffset;
		} else {
			rrDomainName = &_domainName;
		}

		uint32_t ttl = rng.RandomUInt(3600, 86400);

		if (_type == DnsRRType::A) {
			builder.AppendDnsARecord(rrDomainName, ttl, rng.RandomBytes(4));

		} else if (_type == DnsRRType::AAAA) {
			builder.AppendDnsAAAARecord(rrDomainName, ttl, rng.RandomBytes(16));

		} else if (_type == DnsRRType::CNAME) {
			uint64_t len = plan._size - builder.Length() - DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN;
			if (!_useCompression) {
				len -= _domainName.size();
			}
			auto cname = DomainNameGenerator::GetInstance().Generate(len);
			builder.AppendDnsCNAMERecord(rrDomainName, ttl, cname);

		} else if (_type == DnsRRType::TXT) {
			auto len = txtAnswerSizes[i] - DNS_TXT_ANSWER_SIZE_EXCL_DATA;
			builder.AppendDnsTXTRecord(rrDomainName, ttl, rng.RandomString(len));
		}
	}

	auto layer = builder.ToLayer();
	plan._size = 0;
	packet.addLayer(layer.release(), true);

	_domainName.clear();
}

void Dns::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	if (_generateRandomPayloadInsteadOfDns) {
		std::vector<uint8_t> payload = RandomGenerator::GetInstance().RandomBytes(plan._size);
		packet.addLayer(new pcpp::PayloadLayer(payload.data(), payload.size(), true));
		return;
	}

	if (plan._direction == Direction::Forward) {
		BuildForwardPacket(packet, params, plan);
	} else {
		BuildReversePacket(packet, params, plan);
	}
}

} // namespace generator
