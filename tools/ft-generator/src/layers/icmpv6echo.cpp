/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMPv6 layer planner for ping communication
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "icmpv6echo.h"

#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IcmpV6Layer.h>
#include <pcapplusplus/PayloadLayer.h>
#include <pcapplusplus/UdpLayer.h>

#include <cstdlib>
#include <functional>
#include <random>
#include <utility>

namespace generator {

constexpr int MAX_PAYLOAD_SIZE = 1400;

Icmpv6Echo::Icmpv6Echo()
{
	_seqFwd = 1;
	_seqRev = 1;
	_id = 1;
}

void Icmpv6Echo::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);
	Direction dir = Direction::Forward;
	Packet* pkt;

	uint64_t fwdPkts = 0;
	uint64_t revPkts = 0;

	// Plan the directions and the headers
	while (planner.PktsRemaining()) {
		pkt = planner.NextPacket();
		Packet::layerParams params;

		if (dir == Direction::Forward) {
			pkt->_direction = Direction::Forward;
			fwdPkts++;
		} else if (dir == Direction::Reverse) {
			pkt->_direction = Direction::Reverse;
			revPkts++;
		}
		pkt->_layers.emplace_back(this, params);
		pkt->_size += sizeof(pcpp::icmpv6_echo_hdr);
		planner.IncludePkt(pkt);

		if (planner.FwdPktsRemaining() == 0) {
			dir = Direction::Reverse;
		} else if (planner.RevPktsRemaining() == 0) {
			dir = Direction::Forward;
		} else {
			dir = dir == Direction::Forward ? Direction::Reverse : Direction::Forward;
		}
	}

	uint64_t fwdRemBpp = fwdPkts > 0 ? planner.FwdBytesRemaining() / fwdPkts : 0;
	uint64_t revRemBpp = revPkts > 0 ? planner.RevBytesRemaining() / revPkts : 0;

	/* Choose the ping payload size by picking the bigger of the two, we might want to do
		this in a more complex way in the future to fit the flow characteristics better */
	uint64_t payloadSize;
	if (fwdRemBpp > revRemBpp) {
		payloadSize = fwdRemBpp;
	} else {
		payloadSize = revRemBpp;
	}
	if (payloadSize > MAX_PAYLOAD_SIZE) {
		payloadSize = MAX_PAYLOAD_SIZE;
	}

	// Plan the payloads
	planner.Reset();
	while ((pkt = planner.NextPacket())) {
		pkt->_isFinished = true;
		pkt->_size += payloadSize;
	}
}

void Icmpv6Echo::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;

	assert(plan._size >= sizeof(pcpp::icmpv6_echo_hdr));
	plan._size -= sizeof(pcpp::icmpv6_echo_hdr);

	if (_data.size() != plan._size) {
		std::independent_bits_engine<std::default_random_engine, 8, uint8_t> rbe;
		_data.resize(plan._size);
		std::generate(_data.begin(), _data.end(), std::ref(rbe));
	}
	plan._size = 0;

	pcpp::IcmpV6Layer* icmpv6Layer = nullptr;
	if (plan._direction == Direction::Forward) {
		icmpv6Layer = new pcpp::ICMPv6EchoLayer(
			pcpp::ICMPv6EchoLayer::REQUEST,
			_id,
			_seqFwd,
			_data.data(),
			_data.size());
		_seqFwd++;

	} else if (plan._direction == Direction::Reverse) {
		icmpv6Layer = new pcpp::ICMPv6EchoLayer(
			pcpp::ICMPv6EchoLayer::REPLY,
			_id,
			_seqRev,
			_data.data(),
			_data.size());
		_seqRev++;
	}

	packet.addLayer(icmpv6Layer, true);
}

} // namespace generator
