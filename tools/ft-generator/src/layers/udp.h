/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief UDP layer planner
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
 * @brief A representation of an Udp layer.
 */
class Udp : public Layer {
public:
	/**
	 * @brief Construct a new Udp object
	 *
	 * @param portSrc Source port
	 * @param portDst Destination port
	 */
	Udp(uint16_t portSrc, uint16_t portDst);

	/**
	 * @brief Plan Udp layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build Udp layer in packet PcppPacket
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	uint16_t _portSrc;
	uint16_t _portDst;
};

} // namespace generator
