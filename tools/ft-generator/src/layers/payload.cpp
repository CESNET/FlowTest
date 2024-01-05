/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generic payload layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "payload.h"
#include "../packetflowspan.h"

#include <pcapplusplus/PayloadLayer.h>

#include <algorithm>
#include <vector>

namespace generator {

void Payload::PlanFlow(Flow& flow)
{
	PacketFlowSpan packetsSpanAll(&flow, false);

	// Add payload layer to fragmented packets and to unfinished packets.
	for (auto& packet : packetsSpanAll) {
		if (!packet._isFinished) {
			Packet::layerParams params;
			packet._layers.emplace_back(std::make_pair(this, params));
		}
	}
}

void Payload::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) params;

	pcpp::PayloadLayer* payloadLayer;

	// TODO: Might need optimization
	std::vector<uint8_t> payload = RandomGenerator::GetInstance().RandomBytes(plan._size);

	payloadLayer = new pcpp::PayloadLayer(payload.data(), plan._size, false);

	packet.addLayer(payloadLayer, true);
}

} // namespace generator
