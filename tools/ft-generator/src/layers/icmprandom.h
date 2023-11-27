/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMP layer planner using random messages
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"

#include <pcapplusplus/IcmpLayer.h>

namespace generator {

/**
 * @brief A representation of an IcmpRandom layer.
 */
class IcmpRandom : public Layer {
public:
	/**
	 * @brief Construct a new IcmpRandom object
	 *
	 */
	IcmpRandom();

	/**
	 * @brief Plan IcmpRandom layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build IcmpRandom layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	pcpp::IcmpDestUnreachableCodes _destUnreachableCode;
};

} // namespace generator
