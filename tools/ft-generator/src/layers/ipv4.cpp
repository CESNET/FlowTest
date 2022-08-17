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

#include <arpa/inet.h>
#include <pcapplusplus/IPv4Layer.h>

#include <cstdlib>
#include <ctime>
#include <iostream>

namespace generator {

IPv4::IPv4(IPv4Address ipSrc, IPv4Address ipDst) :
	_ipSrc(ipSrc), _ipDst(ipDst)
{
	std::srand(std::time(nullptr));
	_ttl = std::rand() % (256 - 16) + 16;
	_fwdId = std::rand();
	_revId = std::rand();
}

void IPv4::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpan(&flow, true);
	for (auto& packet: packetsSpan) {
		Packet::layerParams params;
		packet._size += pcpp::IPv4Layer().getHeaderLen();
		packet._layers.emplace_back(std::make_pair(this, params));
	}
}

void IPv4::Build(pcpp::Packet& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;

	pcpp::IPv4Layer *ip4Layer;
	pcpp::iphdr *ip4Header;

	if (plan._direction == Direction::Forward) {
		ip4Layer = new pcpp::IPv4Layer(_ipSrc, _ipDst);
	} else {
		ip4Layer = new pcpp::IPv4Layer(_ipDst, _ipSrc);
	}

	ip4Header = ip4Layer->getIPv4Header();
	ip4Header->timeToLive = _ttl;
	if (plan._direction == Direction::Forward) {
		ip4Header->ipId = htons(_fwdId);
		_fwdId++;
	} else {
		ip4Header->ipId = htons(_revId);
		_revId++;
	}

	if (plan._size > ip4Layer->getHeaderLen()) {
		plan._size -= ip4Layer->getHeaderLen();
	} else {
		plan._size = 0;
	}

	packet.addLayer(ip4Layer, true);
}

} // namespace generator
