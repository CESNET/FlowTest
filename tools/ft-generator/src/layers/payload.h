/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generic payload layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../layer.h"
#include "../packet.h"

namespace generator {

/**
 * @brief A representation of a Payload layer.
 */
class Payload : public Layer {
public:

	/**
	 * @brief Plan Payload layer
	 *
	 * @param flow Flow to plan.
	 */
	void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build Payload layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	void Build(pcpp::Packet& packet, Packet::layerParams& params, Packet& plan) override;

private:

};

} // namespace generator
