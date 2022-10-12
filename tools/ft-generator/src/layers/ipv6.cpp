/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ipv4 layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv6.h"
#include "../packetflowspan.h"
#include "../randomgenerator.h"

#include <arpa/inet.h>
#include <pcapplusplus/IPv6Layer.h>

#include <cstdlib>

namespace generator {

IPv6::IPv6(IPv6Address ipSrc, IPv6Address ipDst) :
	_ipSrc(ipSrc), _ipDst(ipDst)
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

	for (auto& packet: packetsSpan) {
		Packet::layerParams params;
		packet._size += pcpp::IPv6Layer().getHeaderLen();
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void IPv6::Build(pcpp::Packet& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;

	pcpp::IPv6Layer *ip6Layer;
	pcpp::ip6_hdr *ip6Header;

	if (plan._direction == Direction::Forward) {
		ip6Layer = new pcpp::IPv6Layer(_ipSrc, _ipDst);
	} else {
		ip6Layer = new pcpp::IPv6Layer(_ipDst, _ipSrc);
	}

	ip6Header = ip6Layer->getIPv6Header();
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

	if (plan._size > ip6Layer->getHeaderLen()) {
		plan._size -= ip6Layer->getHeaderLen();
	} else {
		plan._size = 0;
	}

	packet.addLayer(ip6Layer, true);
}

} // namespace generator
