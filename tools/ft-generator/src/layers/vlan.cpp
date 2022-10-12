/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Vlan layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vlan.h"
#include "ipv4.h"
#include "ipv6.h"
#include "vlan.h"
#include "mpls.h"
#include "../packetflowspan.h"

#include <pcapplusplus/VlanLayer.h>

namespace generator {

Vlan::Vlan(uint16_t vlanId) :
	_vlanId(vlanId)
{
}

void Vlan::PlanFlow(Flow& flow)
{
	Layer* nextLayer = GetNextLayer();
	if (dynamic_cast<IPv4*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_IP;
	} else if (dynamic_cast<IPv6*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_IPV6;
	} else if (dynamic_cast<Vlan*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_VLAN;
	} else if (dynamic_cast<Mpls*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_MPLS;
	} else {
		assert(false && "Unexpected next layer");
	}

	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		Packet::layerParams params;
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void Vlan::PlanExtra(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		if (packet._isExtra) {
			Packet::layerParams params;
			packet._layers.emplace_back(std::make_pair(this, params));
		}
	}
}

void Vlan::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	(void) plan;

	pcpp::VlanLayer *vlanLayer = new pcpp::VlanLayer(_vlanId, false, 0, _etherType);
	packet.addLayer(vlanLayer, true);
}

} // namespace generator
