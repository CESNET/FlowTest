/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mpls layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"

namespace generator {

/**
 * @brief A representation of an Mpls layer.
 */
class Mpls : public Layer {
public:
	/**
	 * @brief Construct a new Mpls object
	 *
	 * @param mplsLabel  The MPLS label
	 */
	Mpls(uint32_t mplsLabel);

	/**
	 * @brief Plan Mpls layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Plan Mpls layer extra packets
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanExtra(Flow& flow) override;

	/**
	 * @brief Build Mpls layer in packet PcppPacket
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	uint32_t _mplsLabel;
	uint8_t _ttl;
	bool _isNextLayerMpls = false;
};

} // namespace generator
