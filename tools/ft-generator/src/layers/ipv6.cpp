/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ipv6 layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv6.h"
#include "../packetflowspan.h"
#include "../randomgenerator.h"
#include "icmpv6echo.h"
#include "icmpv6random.h"

#include <arpa/inet.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Extensions.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/PayloadLayer.h>

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>

namespace generator {

static constexpr int IPV6_BASE_HDR_SIZE = sizeof(pcpp::ip6_hdr);
static constexpr int IPV6_FRAG_HDR_SIZE = sizeof(pcpp::IPv6FragmentationHeader::ipv6_frag_header);

enum class IPv6Map : int { FragmentCount };

IPv6::IPv6(
	IPv6Address ipSrc,
	IPv6Address ipDst,
	double fragmentChance,
	uint16_t minPacketSizeToFragment)
	: _ipSrc(ipSrc)
	, _ipDst(ipDst)
	, _fragmentChance(fragmentChance)
	, _minPacketSizeToFragment(minPacketSizeToFragment)
{
	_ttl = RandomGenerator::GetInstance().RandomUInt(16, 255);

	uint64_t fwdFlowLabel = RandomGenerator::GetInstance().RandomUInt();
	_fwdFlowLabel[0] = fwdFlowLabel & 0xFF;
	_fwdFlowLabel[1] = (fwdFlowLabel >> 8) & 0xFF;
	_fwdFlowLabel[2] = (fwdFlowLabel >> 16) & 0xFF;

	uint64_t revFlowLabel = RandomGenerator::GetInstance().RandomUInt();
	_revFlowLabel[0] = revFlowLabel & 0xFF;
	_revFlowLabel[1] = (revFlowLabel >> 8) & 0xFF;
	_revFlowLabel[2] = (revFlowLabel >> 16) & 0xFF;
}

void IPv6::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet : packetsSpan) {
		Packet::layerParams params;
		packet._size += IPV6_BASE_HDR_SIZE;
		packet._layers.emplace_back(std::make_pair(this, params));
	}

	Layer* nextLayer = GetNextLayer();
	_isIcmpv6Next = dynamic_cast<Icmpv6Echo*>(nextLayer) || dynamic_cast<Icmpv6Random*>(nextLayer);
}

void IPv6::PostPlanFlow(Flow& flow)
{
	for (auto it = flow._packets.begin(); it != flow._packets.end(); it++) {
		if (it->_size < _minPacketSizeToFragment) {
			continue;
		}
		if (RandomGenerator::GetInstance().RandomDouble() <= _fragmentChance) {
			Packet::layerParams& params = GetPacketParams(*it);
			params[int(IPv6Map::FragmentCount)] = uint64_t(2);

			Packet newPkt;
			newPkt._isExtra = true;
			newPkt._direction = it->_direction;
			it = flow._packets.insert(std::next(it), newPkt);
		}
	}
}

void IPv6::PlanExtra(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet : packetsSpan) {
		if (packet._isExtra) {
			Packet::layerParams params;
			packet._size += IPV6_BASE_HDR_SIZE + IPV6_FRAG_HDR_SIZE;
			packet._layers.emplace_back(std::make_pair(this, params));
			packet._isFinished = true;
		}
	}
}

void IPv6::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	pcpp::IPv6Layer* layer;
	if (plan._direction == Direction::Forward) {
		layer = new pcpp::IPv6Layer(_ipSrc, _ipDst);
	} else {
		layer = new pcpp::IPv6Layer(_ipDst, _ipSrc);
	}

	pcpp::ip6_hdr* ip6Header = layer->getIPv6Header();
	ip6Header->hopLimit = _ttl;
	if (plan._direction == Direction::Forward) {
		ip6Header->flowLabel[0] = _fwdFlowLabel[0];
		ip6Header->flowLabel[1] = _fwdFlowLabel[1];
		ip6Header->flowLabel[2] = _fwdFlowLabel[2];
	} else {
		ip6Header->flowLabel[0] = _revFlowLabel[0];
		ip6Header->flowLabel[1] = _revFlowLabel[1];
		ip6Header->flowLabel[2] = _revFlowLabel[2];
	}

	// pcpp::IcmpV6Layer misses ICMPv6 option in its automatic nextHeader
	// setting logic, so we have to do it ourselves
	if (_isIcmpv6Next) {
		ip6Header->nextHeader = pcpp::IPProtocolTypes::PACKETPP_IPPROTO_ICMPV6;
	}

	if (plan._size > layer->getHeaderLen()) {
		plan._size -= layer->getHeaderLen();
	} else {
		plan._size = 0;
	}

	packet.addLayer(layer, true);
	_buildPacketSize = packet.getRawPacketReadOnly()->getRawDataLen();
	if (_fragmentRemaining > 0) {
		// We have to find the layer again since addLayer apparently invalidates the pointer???
		//
		// NOTE: We assume there is only one Ipv6 layer!
		// If there are multiple and we want to fragment one of the lower ones this has to be done a
		// different way.
		layer = static_cast<pcpp::IPv6Layer*>(packet.getLayerOfType(pcpp::IPv6));
		BuildFragment(packet, layer);
	}
}

void IPv6::BuildFragment(PcppPacket& packet, pcpp::IPv6Layer* ipv6Layer)
{
	assert(_fragmentRemaining > 0);

	uint64_t sizeRemaining = _fragmentBuffer.size() - _fragmentOffset;
	uint64_t fragmentSize = _fragmentBuffer.size() / _fragmentCount;
	fragmentSize = (fragmentSize + 7) / 8 * 8;
	if (sizeRemaining < fragmentSize || _fragmentRemaining == 1) {
		fragmentSize = sizeRemaining;
	}

	assert(_fragmentOffset % 8 == 0);
	assert(sizeRemaining >= fragmentSize);

	pcpp::ip6_hdr* ipv6Hdr = ipv6Layer->getIPv6Header();
	// Set protocol before adding extension header, addExtension moves it forward
	ipv6Hdr->nextHeader = _fragmentProto;
	ipv6Hdr->payloadLength = htons(IPV6_FRAG_HDR_SIZE + fragmentSize);

	bool isLastFragment = (_fragmentRemaining <= 1);
	pcpp::IPv6FragmentationHeader fragHeader(_fragmentId, _fragmentOffset, isLastFragment);
	ipv6Layer->addExtension<pcpp::IPv6FragmentationHeader>(fragHeader);

	pcpp::PayloadLayer* payloadLayer
		= new pcpp::PayloadLayer(&_fragmentBuffer[_fragmentOffset], fragmentSize, false);
	packet.addLayer(payloadLayer, true);

	_fragmentOffset += fragmentSize;
	_fragmentRemaining--;
}

void IPv6::PostBuild(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) plan;
	// Check if the packet is start of fragmentation, and if so how many fragments it should be
	// split to
	auto it = params.find(int(IPv6Map::FragmentCount));
	if (it != params.end()) {
		uint64_t fragmentCount = std::get<uint64_t>(it->second);
		assert(_fragmentRemaining == 0);

		_fragmentCount = fragmentCount;
		_fragmentRemaining = _fragmentCount;
		_fragmentId
			= RandomGenerator::GetInstance().RandomUInt(1, std::numeric_limits<uint32_t>::max());

		int dataLen = packet.getRawPacketReadOnly()->getRawDataLen() - _buildPacketSize;
		const uint8_t* data = packet.getRawPacketReadOnly()->getRawData() + _buildPacketSize;
		_fragmentBuffer.resize(dataLen);
		_fragmentOffset = 0;
		std::copy(data, data + dataLen, _fragmentBuffer.begin());

		pcpp::IPv6Layer* layer = static_cast<pcpp::IPv6Layer*>(packet.getLayerOfType(pcpp::IPv6));
		assert(layer != nullptr);
		_fragmentProto = layer->getIPv6Header()->nextHeader;
		packet.removeAllLayersAfter(layer);
		BuildFragment(packet, layer);
	}
}

size_t IPv6::SizeUpToIpLayer(Packet& packet) const
{
	const auto& params = GetPacketParams(packet);
	size_t offset = IPV6_BASE_HDR_SIZE;

	if (params.find(int(IPv6Map::FragmentCount)) != params.end() || packet._isExtra) {
		offset += IPV6_FRAG_HDR_SIZE;
	}

	return offset;
}

} // namespace generator
