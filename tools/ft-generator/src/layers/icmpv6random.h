/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMPv6 layer planner using random messages
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
 * @brief A representation of an Icmpv6Random layer.
 */
class Icmpv6Random : public Layer {
public:
	/**
	 * @brief Construct a new Icmpv6Random object
	 *
	 */
	Icmpv6Random() = default;

	/**
	 * @brief Plan Icmpv6Random layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build Icmpv6Random layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;
};

} // namespace generator
