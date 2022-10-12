/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Vlan layer planner
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
 * @brief A representation of an Vlan layer.
 */
class Vlan : public Layer {
public:
	/**
	 * @brief Construct a new Vlan object
	 *
	 * @param vlanId  The VLAN ID
	 */
	Vlan(uint16_t vlanId);

	/**
	 * @brief Plan Vlan layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Plan Vlan layer extra packets
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanExtra(Flow& flow) override;

	/**
	 * @brief Build Vlan layer in packet PcppPacket
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	uint16_t _vlanId;
	uint16_t _etherType = 0;
};

} // namespace generator
