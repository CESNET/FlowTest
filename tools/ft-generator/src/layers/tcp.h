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
 *
 */
class Tcp : public Layer {
public:
	/**
	 * @brief Construct a new Tcp object
	 *
	 * @param portSrc Source port
	 * @param portDst Destination port
	 */
	Tcp(uint16_t portSrc, uint16_t portDst);

	void PlanFlow(Flow& flow) override;

	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	uint32_t _ackNumber = 0;
	uint32_t _seqNumber = 0;

	uint16_t _portSrc;
	uint16_t _portDst;

	void PlanConnectionHandshake(FlowPlanHelper& planner);
	void PlanTerminationHandshake(FlowPlanHelper& planner);
	void PlanData(FlowPlanHelper& planner);
};

} // namespace generator
