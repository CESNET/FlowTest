/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ethernet layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"

#include <pcapplusplus/MacAddress.h>

namespace generator {

/**
 * @brief A representation of an Ethernet layer.
 */
class Ethernet : public Layer {
public:

	using MacAddress  = pcpp::MacAddress;

	/**
	 * @brief Construct a new Ethernet object
	 *
	 * @param macSrc Source MAC address
	 * @param macDst Destination MAC address
	 */
	Ethernet(MacAddress macSrc, MacAddress macDst);

	/**
	 * @brief Plan Ethernet layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Plan Ethernet layer extra packets
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanExtra(Flow& flow) override;

	/**
	 * @brief Build Ethernet layer in packet pcpp::Packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	MacAddress _macSrc;
	MacAddress _macDst;
	uint16_t _etherType = 0;
};

} // namespace generator
