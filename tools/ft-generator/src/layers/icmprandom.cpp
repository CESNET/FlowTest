/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMP layer planner using random messages
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "icmprandom.h"
#include "../flowplanhelper.h"
#include "../randomgenerator.h"

#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IcmpLayer.h>
#include <pcapplusplus/PayloadLayer.h>
#include <pcapplusplus/UdpLayer.h>

namespace generator {

static constexpr int MAX_DUMMY_PKT_LEN = 1500;
static constexpr int IPV4_HDR_SIZE = sizeof(pcpp::iphdr);
static constexpr int UDP_HDR_SIZE = sizeof(pcpp::udphdr);

static_assert(MAX_DUMMY_PKT_LEN >= IPV4_HDR_SIZE + UDP_HDR_SIZE);

IcmpRandom::IcmpRandom()
{
	int rnd = RandomGenerator::GetInstance().RandomUInt(0, 3);
	if (rnd == 0) {
		_destUnreachableCode = pcpp::IcmpDestinationHostProhibited;
	} else if (rnd == 1) {
		_destUnreachableCode = pcpp::IcmpDestinationNetworkProhibited;
	} else if (rnd == 2) {
		_destUnreachableCode = pcpp::IcmpDestinationHostUnknown;
	} else {
		_destUnreachableCode = pcpp::IcmpDestinationNetworkUnknown;
	}
}

void IcmpRandom::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	while (planner.PktsRemaining()) {
		Packet* pkt = planner.NextPacket();

		Packet::layerParams params;
		pkt->_layers.emplace_back(this, params);
		pkt->_size += sizeof(pcpp::icmp_destination_unreachable);
		pkt->_isFinished = true;

		Direction dir = planner.GetRandomDir();
		pkt->_direction = dir;
		planner.IncludePkt(pkt);
	}
}

void IcmpRandom::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	(void) plan;
	pcpp::IcmpLayer* icmpLayer = new pcpp::IcmpLayer();
	packet.addLayer(icmpLayer, true);

	pcpp::IPv4Layer* thisIpv4Layer
		= static_cast<pcpp::IPv4Layer*>(packet.getLayerOfType(pcpp::IPv4));
	if (!thisIpv4Layer) {
		throw std::runtime_error("ICMP layer cannot find IPv4 header");
	}

	// Create dummy IPv4 and UDP layer to use as the destination unreachable message payload
	pcpp::IPv4Layer* ipv4Layer = new pcpp::IPv4Layer();
	ipv4Layer->setSrcIPv4Address(thisIpv4Layer->getDstIPv4Address());
	ipv4Layer->setDstIPv4Address(thisIpv4Layer->getSrcIPv4Address());
	pcpp::iphdr* ipv4Hdr = ipv4Layer->getIPv4Header();
	ipv4Hdr->timeToLive = RandomGenerator::GetInstance().RandomUInt(1, 255);
	ipv4Hdr->totalLength = RandomGenerator::GetInstance().RandomUInt(
		IPV4_HDR_SIZE + UDP_HDR_SIZE,
		MAX_DUMMY_PKT_LEN);
	ipv4Hdr->protocol = static_cast<uint8_t>(L4Protocol::Udp);

	pcpp::UdpLayer* udpLayer = new pcpp::UdpLayer(
		RandomGenerator::GetInstance().RandomUInt(1, 65535),
		RandomGenerator::GetInstance().RandomUInt(1, 65535));

	// We might want to add more possible ICMP messages in the future
	icmpLayer->setDestUnreachableData(_destUnreachableCode, 0, ipv4Layer, udpLayer);
}

} // namespace generator
