/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Implementation of Flow interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flow.h"
#include "layer.h"
#include "layers/dns.h"
#include "layers/ethernet.h"
#include "layers/http.h"
#include "layers/icmpecho.h"
#include "layers/icmprandom.h"
#include "layers/icmpv6echo.h"
#include "layers/icmpv6random.h"
#include "layers/ipv4.h"
#include "layers/ipv6.h"
#include "layers/mpls.h"
#include "layers/payload.h"
#include "layers/tcp.h"
#include "layers/tls.h"
#include "layers/udp.h"
#include "layers/vlan.h"
#include "logger.h"
#include "packet.h"
#include "packetflowspan.h"
#include "packetsizegenerator.h"
#include "randomgenerator.h"
#include "timestamp.h"
#include "timestampgenerator.h"
#include "utils.h"

#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/IcmpLayer.h>
#include <pcapplusplus/IcmpV6Layer.h>
#include <pcapplusplus/Packet.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace generator {

static constexpr int ICMP_HDR_SIZE = sizeof(pcpp::icmphdr);
static constexpr int ICMPV6_HDR_SIZE = sizeof(pcpp::icmpv6hdr);
static constexpr int IPV4_HDR_SIZE = sizeof(pcpp::iphdr);
static constexpr int IPV6_HDR_SIZE = sizeof(pcpp::ip6_hdr);
static constexpr int TCP_HDR_SIZE = sizeof(pcpp::tcphdr);
static constexpr int UDP_HDR_SIZE = sizeof(pcpp::udphdr);
static constexpr int ICMP_UNREACH_PKT_SIZE = ICMP_HDR_SIZE + IPV4_HDR_SIZE + UDP_HDR_SIZE;
// Unreachable ICMPv6 message includes 4 reserved bytes after the header
static constexpr int ICMPV6_UNREACH_PKT_SIZE = ICMPV6_HDR_SIZE + 4 + IPV6_HDR_SIZE + UDP_HDR_SIZE;

static std::vector<config::EncapsulationLayer>
ChooseEncaps(const std::vector<config::EncapsulationVariant>& variants)
{
	if (variants.empty()) {
		return {};
	}

	double rand = RandomGenerator::GetInstance().RandomDouble();
	double accum = 0.0;
	for (const auto& variant : variants) {
		accum += variant.GetProbability();
		if (rand <= accum) {
			return variant.GetLayers();
		}
	}

	return {};
}

FlowAddresses GenerateAddresses(const FlowProfile& profile, AddressGenerators& addressGenerators)
{
	FlowAddresses res;

	assert(profile._l3Proto != L3Protocol::Unknown);

	// IP addresses
	if (profile._l3Proto == L3Protocol::Ipv4) {
		if (profile._srcIp) {
			assert(profile._srcIp->getType() == IPAddress::AddressType::IPv4AddressType);
			res._srcIp = profile._srcIp->getIPv4();
		} else {
			res._srcIp = addressGenerators.GenerateIPv4();
		}

		if (profile._dstIp) {
			assert(profile._dstIp->getType() == IPAddress::AddressType::IPv4AddressType);
			res._dstIp = profile._dstIp->getIPv4();
		} else {
			res._dstIp = addressGenerators.GenerateIPv4();
		}

	} else if (profile._l3Proto == L3Protocol::Ipv6) {
		if (profile._srcIp) {
			assert(profile._srcIp->getType() == IPAddress::AddressType::IPv6AddressType);
			res._srcIp = profile._srcIp->getIPv6();
		} else {
			res._srcIp = addressGenerators.GenerateIPv6();
		}

		if (profile._dstIp) {
			assert(profile._dstIp->getType() == IPAddress::AddressType::IPv6AddressType);
			res._dstIp = profile._dstIp->getIPv6();
		} else {
			res._dstIp = addressGenerators.GenerateIPv6();
		}
	}

	// MAC addresses
	res._srcMac = addressGenerators.GenerateMac();
	res._dstMac = addressGenerators.GenerateMac();
	while (res._srcMac == res._dstMac) {
		// Configuration checks that there are at least 2 unique mac addresses,
		// so this should always terminate
		res._dstMac = addressGenerators.GenerateMac();
	}

	return res;
}

NormalizedFlowIdentifier
GetNormalizedFlowIdentifier(const FlowProfile& profile, const FlowAddresses& addresses)
{
	return NormalizedFlowIdentifier(
		addresses._srcIp,
		addresses._dstIp,
		profile._srcPort,
		profile._dstPort,
		profile._l4Proto);
}

static bool
ShouldTlsEncrypt(const config::TlsEncryption& encryptionConfig, const FlowProfile& profile)
{
	const auto& never = encryptionConfig.GetNeverEncryptPorts();
	const auto& always = encryptionConfig.GetAlwaysEncryptPorts();
	double probability = encryptionConfig.GetOtherwiseEncryptProbability();

	auto includes = [](const auto& ranges, auto value) {
		return std::any_of(ranges.begin(), ranges.end(), [&](const auto& range) {
			return range.Includes(value);
		});
	};

	if (profile._l4Proto != L4Protocol::Tcp) {
		return false;
	}

	if (includes(always, profile._dstPort)) {
		return true;
	}

	if (includes(never, profile._dstPort)) {
		return false;
	}

	return RandomGenerator::GetInstance().RandomDouble() < probability;
}

Flow::Flow(
	uint64_t id,
	const FlowProfile& profile,
	const FlowAddresses& addresses,
	const config::Config& config)
	: _fwdPackets(profile._packets)
	, _revPackets(profile._packetsRev)
	, _fwdBytes(profile._bytes)
	, _revBytes(profile._bytesRev)
	, _tsFirst(profile._startTime)
	, _tsLast(profile._endTime)
	, _profileFileLineNum(profile._fileLineNum)
	, _id(id)
	, _config(config)
{
	// ==================================== L2 ====================================
	AddLayer(std::make_unique<Ethernet>(addresses._srcMac, addresses._dstMac));

	// Optional L2 encapsulation
	const auto& encapsLayers = ChooseEncaps(config.GetEncapsulation().GetVariants());
	for (size_t i = 0; i < encapsLayers.size(); i++) {
		const auto& layer = encapsLayers[i];
		if (const auto* vlan = std::get_if<config::EncapsulationLayerVlan>(&layer)) {
			AddLayer(std::make_unique<Vlan>(vlan->GetId()));
		} else if (const auto* mpls = std::get_if<config::EncapsulationLayerMpls>(&layer)) {
			AddLayer(std::make_unique<Mpls>(mpls->GetLabel()));
		} else {
			throw std::runtime_error("Invalid encapsulation layer");
		}
	}

	// ==================================== L3 ====================================
	uint64_t headerLengthsFromIpLayer = 0;

	switch (profile._l3Proto) {
	case L3Protocol::Unknown:
		throw std::runtime_error("Unknown L3 protocol");

	case L3Protocol::Ipv4: {
		assert(addresses._srcIp.getType() == IPAddress::AddressType::IPv4AddressType);
		assert(addresses._dstIp.getType() == IPAddress::AddressType::IPv4AddressType);

		AddLayer(std::make_unique<IPv4>(
			addresses._srcIp.getIPv4(),
			addresses._dstIp.getIPv4(),
			config.GetIPv4().GetFragmentationProbability(),
			config.GetIPv4().GetMinPacketSizeToFragment()));

		headerLengthsFromIpLayer += IPV4_HDR_SIZE;
	} break;

	case L3Protocol::Ipv6: {
		assert(addresses._srcIp.getType() == IPAddress::AddressType::IPv6AddressType);
		assert(addresses._dstIp.getType() == IPAddress::AddressType::IPv6AddressType);

		AddLayer(std::make_unique<IPv6>(
			addresses._srcIp.getIPv6(),
			addresses._dstIp.getIPv6(),
			config.GetIPv6().GetFragmentationProbability(),
			config.GetIPv6().GetMinPacketSizeToFragment()));

		headerLengthsFromIpLayer += IPV6_HDR_SIZE;
	} break;
	}

	// ==================================== L4 ====================================
	switch (profile._l4Proto) {
	case L4Protocol::Unknown:
		throw std::runtime_error("Unknown L4 protocol");

	case L4Protocol::Tcp: {
		AddLayer(std::make_unique<Tcp>(profile._srcPort, profile._dstPort));
		headerLengthsFromIpLayer += TCP_HDR_SIZE;
	} break;

	case L4Protocol::Udp: {
		AddLayer(std::make_unique<Udp>(profile._srcPort, profile._dstPort));
		headerLengthsFromIpLayer += UDP_HDR_SIZE;
	} break;

	case L4Protocol::Icmp: {
		if (profile._l3Proto != L3Protocol::Ipv4) {
			throw std::runtime_error("L4 protocol is ICMP but L3 protocol is not IPv4");
		}
		AddLayer(MakeIcmpLayer(profile._l3Proto));
	} break;

	case L4Protocol::Icmpv6: {
		if (profile._l3Proto != L3Protocol::Ipv6) {
			throw std::runtime_error("L4 protocol is ICMPv6 but L3 protocol is not IPv6");
		}
		AddLayer(MakeIcmpLayer(profile._l3Proto));
	} break;
	}

	// ================================= Payload ==================================
	const auto& enabledProtocols = _config.GetPayload().GetEnabledProtocols();

	if (enabledProtocols.Includes(config::PayloadProtocol::Tls)
		&& ShouldTlsEncrypt(_config.GetPayload().GetTlsEncryption(), profile)) {
		uint64_t maxTlsPayloadSize = _config.GetPacketSizeProbabilities().MaxNormalizedPacketSize();
		maxTlsPayloadSize = SafeSub(maxTlsPayloadSize, headerLengthsFromIpLayer);
		AddLayer(std::make_unique<Tls>(maxTlsPayloadSize));

	} else if (
		enabledProtocols.Includes(config::PayloadProtocol::Http)
		&& profile._l4Proto == L4Protocol::Tcp
		&& (profile._dstPort == 80 || profile._dstPort == 8080)) {
		AddLayer(std::make_unique<Http>());

	} else if (
		enabledProtocols.Includes(config::PayloadProtocol::Dns)
		&& profile._l4Proto == L4Protocol::Udp && profile._dstPort == 53) {
		AddLayer(std::make_unique<Dns>());

	} else if (profile._l4Proto == L4Protocol::Tcp || profile._l4Proto == L4Protocol::Udp) {
		AddLayer(std::make_unique<Payload>());
	}

	// ============================================================================

	Plan();
}

std::unique_ptr<Layer> Flow::MakeIcmpLayer(L3Protocol l3Proto)
{
	double fwdRevRatioDiff = 1.0;
	double bytesPerPkt = 0;
	if (_fwdPackets + _revPackets > 0) {
		double min = std::min(_fwdPackets, _revPackets);
		double max = std::max(_fwdPackets, _revPackets);
		fwdRevRatioDiff = 1.0 - (min / max);
		bytesPerPkt = (_fwdBytes + _revBytes) / (_fwdPackets + _revPackets);
	}

	/*
	 * A simple heuristic to choose the proper ICMP packet generation strategy based
	 * on the flow characteristics
	 *
	 * NOTE: Might need further evaluation if this is a "good enough" way to do this
	 * and/or some tweaking
	 */
	assert(l3Proto == L3Protocol::Ipv4 || l3Proto == L3Protocol::Ipv6);
	std::unique_ptr<Layer> layer;
	if (l3Proto == L3Protocol::Ipv4) {
		if ((_fwdPackets <= 3 || _revPackets <= 3)
			&& (bytesPerPkt <= 1.10 * ICMP_UNREACH_PKT_SIZE)) {
			// Low amount of small enough packets
			layer = std::make_unique<IcmpRandom>();
		} else if (fwdRevRatioDiff <= 0.2) {
			// About the same number of packets in both directions
			layer = std::make_unique<IcmpEcho>();
		} else if (bytesPerPkt <= 1.10 * ICMP_UNREACH_PKT_SIZE) {
			// Small enough packets
			layer = std::make_unique<IcmpRandom>();
		} else {
			// Enough packets and many of them
			layer = std::make_unique<IcmpEcho>();
		}
	} else {
		if ((_fwdPackets <= 3 || _revPackets <= 3)
			&& (bytesPerPkt <= 1.10 * ICMPV6_UNREACH_PKT_SIZE)) {
			// Low amount of small enough packets
			layer = std::make_unique<Icmpv6Random>();
		} else if (fwdRevRatioDiff <= 0.2) {
			// About the same number of packets in both directions
			layer = std::make_unique<Icmpv6Echo>();
		} else if (bytesPerPkt <= 1.10 * ICMPV6_UNREACH_PKT_SIZE) {
			// Small enough packets
			layer = std::make_unique<Icmpv6Random>();
		} else {
			// Enough packets and many of them
			layer = std::make_unique<Icmpv6Echo>();
		}
	}

	return layer;
}

Flow::~Flow()
{
	// NOTE: Explicit destructor is required because the class contains
	//       std::unique_ptr<Layer> where Layer is an incomplete type!
}

void Flow::AddLayer(std::unique_ptr<Layer> layer)
{
	layer->AddedToFlow(this, _layerStack.size());
	_layerStack.emplace_back(std::move(layer));
}

void Flow::Plan()
{
	_packets.resize(_fwdPackets + _revPackets);

	for (auto& layer : _layerStack) {
		layer->PlanFlow(*this);
	}

	PlanPacketsDirections();
	PlanPacketsSizes();

	for (auto& layer : _layerStack) {
		layer->PostPlanFlow(*this);
	}

	for (auto& layer : _layerStack) {
		layer->PlanExtra(*this);
	}

	PlanPacketsTimestamps();
}

PacketExtraInfo Flow::GenerateNextPacket(PcppPacket& packet)
{
	if (_packets.empty()) {
		throw std::runtime_error("no more packets to generate in flow");
	}

	packet = PcppPacket();
	PacketExtraInfo extra;

	Packet packetPlan = _packets.front();
	extra._direction = packetPlan._direction;
	extra._time = packetPlan._timestamp;

	for (auto& [layer, params] : packetPlan._layers) {
		layer->Build(packet, params, packetPlan);
	}
	/**
	 * The method computeCalculateFields needs to be called twice here.
	 * The first time before calling the PostBuild callbacks, as they need the
	 * finished packet including the computed fields.
	 * The second time after calling the PostBuild callbacks, as they might
	 * modify the packet and the fields may need to be recomputed.
	 */
	packet.computeCalculateFields();

	for (auto& [layer, params] : packetPlan._layers) {
		layer->PostBuild(packet, params, packetPlan);
	}
	packet.computeCalculateFields();

	_packets.erase(_packets.begin());

	return extra;
}

Timestamp Flow::GetNextPacketTime() const
{
	return _packets.front()._timestamp;
}

bool Flow::IsFinished() const
{
	return _packets.empty();
}

void Flow::PlanPacketsDirections()
{
	PacketFlowSpan packetsSpan(this, true);
	auto [fwd, rev] = packetsSpan.GetAvailableDirections();

	std::vector<Direction> directions;
	directions.insert(directions.end(), fwd, Direction::Forward);
	directions.insert(directions.end(), rev, Direction::Reverse);

	RandomGenerator::GetInstance().Shuffle(directions);

	size_t id = 0;
	for (auto& packet : packetsSpan) {
		if (packet._direction == Direction::Unknown) {
			packet._direction = directions[id++];
		}
	}
}

void Flow::PlanPacketsTimestamps()
{
	const auto& timestamps = GenerateTimestamps(
		_packets.size(),
		_tsFirst,
		_tsLast,
		_config.GetMaxFlowInterPacketGapSecs());

	auto it = timestamps.begin();
	for (auto& pkt : _packets) {
		pkt._timestamp = Timestamp::From<TimeUnit::Nanoseconds>(*it);
		it++;
	}

	if (_packets.size() > 0 && _packets.back()._timestamp != _tsLast) {
		Timestamp newTsLast = _packets.back()._timestamp;

		_logger->info(
			"Flow (line no. {}) has been trimmed by {}s to satisfy max gap",
			_profileFileLineNum,
			(_tsLast - newTsLast).ToString<TimeUnit::Seconds>());

		_tsLast = newTsLast;
	}
}

void Flow::PlanPacketsSizes()
{
	const auto& intervals = _config.GetPacketSizeProbabilities().AsNormalizedIntervals();
	auto fwdGen = PacketSizeGenerator::Construct(intervals, _fwdPackets, _fwdBytes);
	auto revGen = PacketSizeGenerator::Construct(intervals, _revPackets, _revBytes);

	for (auto& packet : _packets) {
		if (packet._isFinished) {
			auto& generator = packet._direction == Direction::Forward ? fwdGen : revGen;
			generator->GetValueExact(packet._size);
		}
	}

	fwdGen->PlanRemaining();
	revGen->PlanRemaining();

	for (auto& packet : _packets) {
		if (!packet._isFinished) {
			auto& generator = packet._direction == Direction::Forward ? fwdGen : revGen;
			packet._size = std::max(
				packet._size,
				generator->GetValue()); // NOTE: Add the option to GetValue to choose minimum size?
		}
	}

	fwdGen->PrintReport();
	revGen->PrintReport();
}

} // namespace generator
