/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMPv6 layer planner for ping communication
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../flowplanhelper.h"
#include "../layer.h"
#include "../packet.h"
#include "../packetflowspan.h"
#include "../pcpppacket.h"

#include <cstdint>
#include <queue>
#include <vector>

namespace generator {

/**
 * @brief A representation of an Icmpv6Echo layer.
 */
class Icmpv6Echo : public Layer {
public:
	/**
	 * @brief Construct a new Icmpv6Echo object
	 *
	 */
	Icmpv6Echo();

	/**
	 * @brief Plan Icmpv6Echo layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build Icmpv6Echo layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	uint16_t _id;
	uint16_t _seqFwd;
	uint16_t _seqRev;
	std::vector<uint8_t> _data;
};

} // namespace generator
