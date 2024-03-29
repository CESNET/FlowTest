/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMP layer planner for ping communication
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "icmpecho.h"

#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IcmpLayer.h>
#include <pcapplusplus/PayloadLayer.h>
#include <pcapplusplus/UdpLayer.h>

#include <cstdlib>
#include <functional>
#include <utility>

namespace generator {

constexpr int MAX_PAYLOAD_SIZE = 1400;

IcmpEcho::IcmpEcho()
{
	_seqFwd = 1;
	_seqRev = 1;
	_id = 1;
}

void IcmpEcho::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);
	Direction dir = Direction::Forward;
	Packet* pkt;

	uint64_t fwdPkts = 0;
	uint64_t revPkts = 0;

	// Plan the directions and the headers
	while (planner.PktsRemaining() > 0) {
		if (planner.PktsRemaining(dir) == 0) {
			dir = SwapDirection(dir);
			assert(planner.PktsRemaining(dir) > 0);
		}

		pkt = planner.NextPacket();

		if (dir == Direction::Forward) {
			fwdPkts++;
		} else if (dir == Direction::Reverse) {
			revPkts++;
		}
		pkt->_direction = dir;
		pkt->_layers.emplace_back(this, Packet::layerParams {});
		pkt->_size += sizeof(pcpp::icmp_echo_hdr);
		planner.IncludePkt(pkt);

		dir = SwapDirection(dir);
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

void IcmpEcho::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;
	pcpp::IcmpLayer* icmpLayer = new pcpp::IcmpLayer();

	assert(plan._size >= sizeof(pcpp::icmp_echo_hdr));
	plan._size -= sizeof(pcpp::icmp_echo_hdr);

	if (_data.size() != plan._size) {
		_data = RandomGenerator::GetInstance().RandomBytes(plan._size);
	}
	plan._size = 0;

	if (plan._direction == Direction::Forward) {
		icmpLayer->setEchoRequestData(_id, _seqFwd, 0, _data.data(), _data.size());
		_seqFwd++;

	} else if (plan._direction == Direction::Reverse) {
		icmpLayer->setEchoReplyData(_id, _seqRev, 0, _data.data(), _data.size());
		_seqRev++;
	}

	packet.addLayer(icmpLayer, true);
}

} // namespace generator
