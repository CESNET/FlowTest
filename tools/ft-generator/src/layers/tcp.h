/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TCP protocol planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../flowplanhelper.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"

namespace generator {

class FlowPlanHelper;

/**
 * @brief A representation of a TCP layer.
 */
class Tcp : public Layer {
public:
	/**
	 * @brief Construct a new TCP layer object
	 *
	 * @param portSrc Source port
	 * @param portDst Destination port
	 */
	Tcp(uint16_t portSrc, uint16_t portDst);

	/**
	 * @brief Plan TCP layer
	 *
	 * @param flow Flow to plan.
	 */
	void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build TCP layer for packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

	/**
	 * @brief Size from the beginning of the IP layer.
	 *
	 * Represents the number of bytes occupied by this layer and possibly all
	 * previous layers up to and including the beginning of the IP layer. In other
	 * words, the value also represents the offset of the end of this layer from
	 * the beginning of the IP layer.
	 *
	 * @param packet The packet
	 *
	 * @return The offset
	 */
	size_t SizeUpToIpLayer(Packet& packet) const override;

private:
	uint32_t _ackNumber = 0;
	uint32_t _seqNumber = 0;

	uint16_t _portSrc;
	uint16_t _portDst;

	bool _shouldPlanConnHandshake = true;
	bool _shouldPlanTermHandshake = true;

	uint64_t CalcMaxBytesPerPkt();
	void DetermineIfHandshakesShouldBePlanned(FlowPlanHelper& planner);
	void PlanConnectionHandshake(FlowPlanHelper& planner);
	void PlanTerminationHandshake(FlowPlanHelper& planner);
	void PlanData(FlowPlanHelper& planner);
};

} // namespace generator
