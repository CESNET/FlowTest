/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mpls layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mpls.h"
#include "../packetflowspan.h"

#include <pcapplusplus/MplsLayer.h>

namespace generator {

Mpls::Mpls(uint32_t mplsLabel) :
	_mplsLabel(mplsLabel),
	_ttl(RandomGenerator::GetInstance().RandomUInt(16, 255))
{
}

void Mpls::PlanFlow(Flow& flow)
{
	_isNextLayerMpls = (dynamic_cast<Mpls*>(GetNextLayer()) != nullptr);

	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		Packet::layerParams params;
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void Mpls::PlanExtra(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		if (packet._isExtra) {
			Packet::layerParams params;
			packet._layers.emplace_back(std::make_pair(this, params));
		}
	}
}

void Mpls::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	(void) plan;

	pcpp::MplsLayer* mplsLayer = new pcpp::MplsLayer(_mplsLabel, _ttl, 0, !_isNextLayerMpls);
	packet.addLayer(mplsLayer, true);
}

} // namespace generator
