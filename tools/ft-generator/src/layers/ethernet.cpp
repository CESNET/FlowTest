/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ethernet layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ethernet.h"
#include "../packetflowspan.h"

#include <pcapplusplus/EthLayer.h>

namespace generator {

Ethernet::Ethernet(MacAddress macSrc, MacAddress macDst) :
	_macSrc(macSrc), _macDst(macDst)
{
}

void Ethernet::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);

	for (auto& packet: packetsSpan) {
		Packet::layerParams params;
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void Ethernet::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	pcpp::EthLayer *ethLayer;

	(void) params;

	if (plan._direction == Direction::Forward) {
		ethLayer = new pcpp::EthLayer(_macSrc, _macDst);
	} else {
		ethLayer = new pcpp::EthLayer(_macDst, _macSrc);
	}

	packet.addLayer(ethLayer, true);
}

} // namespace generator
