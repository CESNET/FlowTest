/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ethernet layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ethernet.h"
#include "ipv4.h"
#include "ipv6.h"
#include "vlan.h"
#include "mpls.h"
#include "../packetflowspan.h"
#include "../pcppethlayer.h"

#include <arpa/inet.h>

#include <cassert>

namespace generator {

Ethernet::Ethernet(MacAddress macSrc, MacAddress macDst) :
	_macSrc(macSrc), _macDst(macDst)
{
}

void Ethernet::PlanFlow(Flow& flow)
{
	Layer* nextLayer = GetNextLayer();
	if (dynamic_cast<IPv4*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_IP;
	} else if (dynamic_cast<IPv6*>(nextLayer)) {
		_etherType = PCPP_ETHERTYPE_IPV6;
	} else if (auto* vlanLayer = dynamic_cast<Vlan*>(nextLayer)) {
		if (dynamic_cast<Vlan*>(vlanLayer->GetNextLayer())) {
			_etherType = PCPP_ETHERTYPE_IEEE_802_1AD;
		} else {
			_etherType = PCPP_ETHERTYPE_VLAN;
		}
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

void Ethernet::PlanExtra(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		if (packet._isExtra) {
			Packet::layerParams params;
			packet._layers.emplace_back(std::make_pair(this, params));
		}
	}
}

void Ethernet::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	PcppEthLayer *ethLayer;

	(void) params;

	if (plan._direction == Direction::Forward) {
		ethLayer = new PcppEthLayer(_macSrc, _macDst);
	} else {
		ethLayer = new PcppEthLayer(_macDst, _macSrc);
	}
	ethLayer->getEthHeader()->etherType = htons(_etherType);

	packet.addLayer(ethLayer, true);
}

} // namespace generator
