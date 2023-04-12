/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMPv6 layer planner using random messages
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "icmpv6random.h"
#include "../randomgenerator.h"

#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/IcmpV6Layer.h>
#include <pcapplusplus/UdpLayer.h>

#include <cstdlib>
#include <functional>
#include <utility>
#include <vector>

namespace generator {

static constexpr int MAX_DUMMY_PKT_LEN = 1500;
static constexpr int ICMPV6_HDR_SIZE = sizeof(pcpp::icmpv6hdr);
static constexpr int IPV6_HDR_SIZE = sizeof(pcpp::ip6_hdr);
static constexpr int UDP_HDR_SIZE = sizeof(pcpp::udphdr);

enum Icmpv6DestUnreachableCodes : uint8_t {
	NoRoute = 0, // No route to destination
	Prohibited = 1, // Communication with destination administratively prohibited
	BeyondScope = 2, // Beyond scope of source address
	AddrUnreachable = 3, // Address unreachable
	PortUnreachable = 4, // Port unreachable
	SourceAddrPolicyFailed = 5, // Source address failed ingress/egress policy
	RejectRoute = 6, // Reject route to destination
	ErrorInSrcRoutingHdr = 7, // Error in Source Routing Header
	HdrTooLong = 8 // Headers too long
};

static_assert(MAX_DUMMY_PKT_LEN >= IPV6_HDR_SIZE + UDP_HDR_SIZE);

void Icmpv6Random::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	while (planner.PktsRemaining()) {
		Packet* pkt = planner.NextPacket();

		Packet::layerParams params;
		pkt->_layers.emplace_back(this, params);
		pkt->_size += ICMPV6_HDR_SIZE + IPV6_HDR_SIZE + UDP_HDR_SIZE;
		pkt->_isFinished = true;

		Direction dir = planner.GetRandomDir();
		pkt->_direction = dir;
		planner.IncludePkt(pkt);
	}
}

void Icmpv6Random::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	(void) plan;

	pcpp::IPv6Layer* thisIpv6Layer
		= static_cast<pcpp::IPv6Layer*>(packet.getLayerOfType(pcpp::IPv6));
	if (!thisIpv6Layer) {
		throw std::runtime_error("ICMPv6 layer cannot find IPv6 header");
	}

	// Create dummy IPv6 and UDP layer to use as the destination unreachable message payload
	pcpp::IPv6Layer ipv6Layer;
	ipv6Layer.setSrcIPv6Address(thisIpv6Layer->getDstIPv6Address());
	ipv6Layer.setDstIPv6Address(thisIpv6Layer->getSrcIPv6Address());
	pcpp::ip6_hdr* ipv6Hdr = ipv6Layer.getIPv6Header();
	ipv6Hdr->hopLimit = RandomGenerator::GetInstance().RandomUInt(1, 255);
	ipv6Hdr->payloadLength
		= RandomGenerator::GetInstance().RandomUInt(UDP_HDR_SIZE, MAX_DUMMY_PKT_LEN);
	ipv6Hdr->nextHeader = static_cast<uint8_t>(L4Protocol::Udp);

	pcpp::UdpLayer udpLayer(
		RandomGenerator::GetInstance().RandomUInt(1, 65535),
		RandomGenerator::GetInstance().RandomUInt(1, 65535));

	// Compose a dummy packet from the dummy layers and get its data
	PcppPacket dummyPacket;
	dummyPacket.addLayer(&ipv6Layer);
	dummyPacket.addLayer(&udpLayer);
	dummyPacket.computeCalculateFields();
	auto* rawDummyPkt = dummyPacket.getRawPacketReadOnly();

	// First 4 bytes of the ICMPv6 unreachable payload are reserved,
	// so we have to use an extra buffer to put those at the beginning of the data
	std::vector<uint8_t> buf(rawDummyPkt->getRawDataLen() + 4);
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	std::memcpy(&buf[4], rawDummyPkt->getRawData(), rawDummyPkt->getRawDataLen());

	// We might want to add more possible ICMP messages in the future
	int rnd = RandomGenerator::GetInstance().RandomUInt(0, 3);
	Icmpv6DestUnreachableCodes code;
	if (rnd == 0) {
		code = Icmpv6DestUnreachableCodes::NoRoute;
	} else if (rnd == 1) {
		code = Icmpv6DestUnreachableCodes::Prohibited;
	} else if (rnd == 2) {
		code = Icmpv6DestUnreachableCodes::AddrUnreachable;
	} else {
		code = Icmpv6DestUnreachableCodes::PortUnreachable;
	}

	// Construct the ICMPv6 layer using the payload from the dummy packet
	pcpp::IcmpV6Layer* icmpV6Layer = new pcpp::IcmpV6Layer(
		pcpp::ICMPv6MessageType::ICMPv6_DESTINATION_UNREACHABLE,
		code,
		&buf[0],
		buf.size());
	packet.addLayer(icmpV6Layer, true);
}

} // namespace generator
