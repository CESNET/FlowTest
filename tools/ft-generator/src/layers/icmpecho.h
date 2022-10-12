/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMP layer planner for ping communication
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

#include <pcapplusplus/IcmpLayer.h>

#include <cstdint>
#include <vector>
#include <queue>

namespace generator {

/**
 * @brief A representation of an IcmpEcho layer.
 */
class IcmpEcho : public Layer {
public:

	/**
	 * @brief Construct a new IcmpEcho object
	 *
	 */
	IcmpEcho();

	/**
	 * @brief Plan IcmpEcho layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build IcmpEcho layer in packet
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
