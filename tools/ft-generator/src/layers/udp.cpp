/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief UDP layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "udp.h"
#include "../packetflowspan.h"

#include <pcapplusplus/UdpLayer.h>

static constexpr int UDP_HDR_LEN = sizeof(pcpp::udphdr);

namespace generator {

Udp::Udp(uint16_t portSrc, uint16_t portDst)
	: _portSrc(portSrc)
	, _portDst(portDst)
{
}

void Udp::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);

	for (auto& packet : packetsSpan) {
		Packet::layerParams params;
		packet._size += UDP_HDR_LEN;
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void Udp::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	pcpp::UdpLayer* udpLayer;

	(void) params;

	if (plan._direction == Direction::Forward) {
		udpLayer = new pcpp::UdpLayer(_portSrc, _portDst);
	} else {
		udpLayer = new pcpp::UdpLayer(_portDst, _portSrc);
	}

	assert(plan._size >= UDP_HDR_LEN);
	plan._size -= UDP_HDR_LEN;

	packet.addLayer(udpLayer, true);
}

} // namespace generator
