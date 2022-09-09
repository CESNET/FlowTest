/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Layer planner interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "packet.h"

#include <pcapplusplus/Packet.h>

#include <iostream>

namespace generator {

class Flow;

/**
 * @brief Class representing the layer interface.
 */
class Layer {
public:

	/**
	 * @brief Plan packets in flow from bottom to top layer.
	 *
	 * @param flow Flow to plan
	 */
	virtual void PlanFlow(Flow& flow) = 0;

	/**
	 * @brief Build pcpp::Packet layer.
	 *
	 * @param packet Packet to build
	 * @param params Parameters of current layer plan.
	 * @param plan   Packet plan.
	 */
	virtual void Build(pcpp::Packet& packet, Packet::layerParams& params, Packet& plan) = 0;
};

} // namespace generator
