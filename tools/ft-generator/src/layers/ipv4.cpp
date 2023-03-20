/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ipv4 layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv4.h"
#include "../packetflowspan.h"
#include "../randomgenerator.h"

#include <arpa/inet.h>
#include <pcapplusplus/PayloadLayer.h>

#include <cstdlib>
#include <ctime>
#include <iostream>

namespace generator {

enum class IPv4Map : int { FragmentCount };

constexpr int IPV4_HDR_SIZE = sizeof(pcpp::iphdr);

IPv4::IPv4(
	IPv4Address ipSrc,
	IPv4Address ipDst,
	double fragmentChance,
	uint16_t minPacketSizeToFragment)
	: _ipSrc(ipSrc)
	, _ipDst(ipDst)
	, _fragmentChance(fragmentChance)
	, _minPacketSizeToFragment(minPacketSizeToFragment)
{
	_ttl = RandomGenerator::GetInstance().RandomUInt(16, 255);
	_fwdId = RandomGenerator::GetInstance().RandomUInt();
	_revId = RandomGenerator::GetInstance().RandomUInt();
}

void IPv4::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet : packetsSpan) {
		Packet::layerParams params;
		packet._size += pcpp::IPv4Layer().getHeaderLen();
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void IPv4::PostPlanFlow(Flow& flow)
{
	for (auto it = flow._packets.begin(); it != flow._packets.end(); it++) {
		if (it->_size < _minPacketSizeToFragment) {
			continue;
		}
		if (RandomGenerator::GetInstance().RandomDouble() <= _fragmentChance) {
			Packet::layerParams& params = GetPacketParams(*it);
			params[int(IPv4Map::FragmentCount)] = uint64_t(2);

			Packet newPkt;
			newPkt._isExtra = true;
			newPkt._direction = it->_direction;
			it = flow._packets.insert(std::next(it), newPkt);
		}
	}
}

void IPv4::PlanExtra(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet : packetsSpan) {
		if (packet._isExtra) {
			Packet::layerParams params;
			packet._size += pcpp::IPv4Layer().getHeaderLen();
			packet._layers.emplace_back(std::make_pair(this, params));
			packet._isFinished = true;
		}
	}
}

void IPv4::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	pcpp::IPv4Layer* layer;
	if (plan._direction == Direction::Forward) {
		layer = new pcpp::IPv4Layer(_ipSrc, _ipDst);
	} else {
		layer = new pcpp::IPv4Layer(_ipDst, _ipSrc);
	}

	pcpp::iphdr* ip4Header = layer->getIPv4Header();
	ip4Header->timeToLive = _ttl;
	if (plan._direction == Direction::Forward) {
		ip4Header->ipId = htons(_fwdId);
		_fwdId++;
	} else {
		ip4Header->ipId = htons(_revId);
		_revId++;
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
		// NOTE: We assume there is only one Ipv4 layer!
		// If there are multiple and we want to fragment one of the lower ones this has to be done a
		// different way.
		layer = static_cast<pcpp::IPv4Layer*>(packet.getLayerOfType(pcpp::IPv4));
		BuildFragment(packet, layer->getIPv4Header());
	}
}

void IPv4::BuildFragment(PcppPacket& packet, pcpp::iphdr* ipHdr)
{
	assert(_fragmentRemaining > 0);

	uint64_t sizeRemaining = _fragmentBuffer.size() - _fragmentOffset;
	uint64_t fragmentSize = _fragmentBuffer.size() / _fragmentCount;
	fragmentSize = (fragmentSize + 7) / 8 * 8;
	fragmentSize = std::min<uint64_t>(fragmentSize, sizeRemaining);

	assert(_fragmentOffset % 8 == 0);
	assert(sizeRemaining >= fragmentSize);

	ipHdr->fragmentOffset = htons(_fragmentOffset / 8);
	if (_fragmentRemaining > 1) {
		ipHdr->fragmentOffset |= PCPP_IP_MORE_FRAGMENTS;
	}
	ipHdr->ipId = htons(_fragmentId);
	ipHdr->totalLength = htons(IPV4_HDR_SIZE + fragmentSize);
	ipHdr->protocol = _fragmentProto;
	pcpp::PayloadLayer* payloadLayer
		= new pcpp::PayloadLayer(&_fragmentBuffer[_fragmentOffset], fragmentSize, false);
	packet.addLayer(payloadLayer, true);

	_fragmentOffset += fragmentSize;
	_fragmentRemaining--;
}

void IPv4::PostBuild(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) plan;
	// Check if the packet is start of fragmentation, and if so how many fragments it should be
	// split to
	auto it = params.find(int(IPv4Map::FragmentCount));
	if (it != params.end()) {
		uint64_t fragmentCount = std::get<uint64_t>(it->second);
		assert(_fragmentRemaining == 0);

		_fragmentCount = fragmentCount;
		_fragmentRemaining = _fragmentCount;
		_fragmentId = RandomGenerator::GetInstance().RandomUInt(1, 65535);

		int dataLen = packet.getRawPacketReadOnly()->getRawDataLen() - _buildPacketSize;
		const uint8_t* data = packet.getRawPacketReadOnly()->getRawData() + _buildPacketSize;
		_fragmentBuffer.resize(dataLen);
		_fragmentOffset = 0;
		std::copy(data, data + dataLen, _fragmentBuffer.begin());

		pcpp::IPv4Layer* layer = static_cast<pcpp::IPv4Layer*>(packet.getLayerOfType(pcpp::IPv4));
		assert(layer != nullptr);
		_fragmentProto = layer->getIPv4Header()->protocol;
		packet.removeAllLayersAfter(layer);
		BuildFragment(packet, layer->getIPv4Header());
	}
}

} // namespace generator
