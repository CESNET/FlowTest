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
#include "pcpppacket.h"

#include <iostream>

namespace generator {

class Flow;

/**
 * @brief Class representing the layer interface.
 */
class Layer {
public:
	/**
	 * @brief Called when the layer is added to a flow
	 *
	 * @param flow      The flow
	 * @param layerNumber  The number of the layer in the flow's network stack
	 */
	void AddedToFlow(Flow* flow, size_t layerNumber);

	/**
	 * @brief Plan packets in flow from bottom to top layer.
	 *
	 * @param flow Flow to plan
	 */
	virtual void PlanFlow(Flow& flow) = 0;

	/**
	 * @brief Called after the flow packets are planned for further processing by the layer
	 *
	 * @param flow  The flow
	 */
	virtual void PostPlanFlow(Flow& flow);

	/**
	 * @brief Called to plan extra packets that may have appeared in the flow
	 *
	 * @param flow  The flow
	 */
	virtual void PlanExtra(Flow& flow);

	/**
	 * @brief Build PcppPacket layer.
	 *
	 * @param packet Packet to build
	 * @param params Parameters of current layer plan.
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) = 0;

	/**
	 * @brief Called after the packet is built for additional processing
	 *
	 * @param packet  The built packet
	 * @param params  The packet parameters
	 * @param plan    The packet plan
	 */
	virtual void PostBuild(PcppPacket& packet, Packet::layerParams& params, Packet& plan);

protected:
	/**
	 * @brief Get the Flow this layer belongs to
	 */
	Flow* GetFlow() const { return _flow; }

	/**
	 * @brief Get the layer number (index of the layer) in the Flow's layer stack
	 */
	size_t GetLayerNumber() const { return _layerNumber; };

	/**
	 * @brief Get the Next Layer in the flow layer stack
	 *
	 * @return The layer or nullptr if this is the last layer,
	 *         or the layer is not associated with a flow
	 */
	Layer* GetNextLayer() const;

	/**
	 * @brief Get the packet parameters
	 *
	 * @param packet  The packet
	 * @return Packet::layerParams&  The packet parameters
	 */
	Packet::layerParams& GetPacketParams(Packet& packet) const;

private:
	/**
	 * @brief The flow this layer belongs to
	 *
	 * @note Set by the Flow when the layer is added and already available when
	 *       the public methods are called
	 */
	Flow* _flow = nullptr;

	/**
	 * @brief The index of the layer in the flow layer stack,
	 *
	 * @note Set by the Flow when the layer is added and already available when
	 *       the public methods are called
	 */
	size_t _layerNumber = ~size_t(0);

};

} // namespace generator
