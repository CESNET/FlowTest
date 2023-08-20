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
 *
 * Creation:
 *
 *   Layers are created by a Flow and added to its layer stack.
 *
 *   - Layer is created through construction of one of the derived concrete
 *     layers.
 *   - Layer is added to a layer stack of a flow upon which the AddedToFlow
 *     callback is called.
 *
 * Planning:
 *
 *   The planning phase happens after all the layers of the flow are created and
 *   present in the flow.
 *
 *   - PlanFlow callback is called to plan all the packets of this layer
 *   - PostPlanFlow callback is called in case the layer needs to perform
 *     further operations after all the packets are planned. This happens after
 *     all the layers have planned all the packets.
 *   - PlanExtra callback is called in case there any extra packets were added
 *     by other layers that require further planning, because they were not
 *     present in the PlanFlow phase. This happens after all the layers have
 *     gone through their PostPlanFlow phase.
 *
 * Building:
 *
 *   The building phase happens after the planning phase has finished in all the
 *   layers of the flow, meaning all the packets have all their layers planned.
 *   The building phase is then performed for each packet that needs to be
 *   generated. This is done for each packet individually.
 *
 *   - Build callback is called to build a packet according to its plan and
 *     parameters provided in planning phase
 *
 *   - PostBuild callback is called after all the layers of the packet are
 *     built. This allows layers to observe or even further modify the built
 *     packet.
 *
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

	/**
	 * @brief Get the Previous Layer in the flow layer stack
	 *
	 * @return The layer or nullptr if this is the first layer,
	 *         or the layer is not associated with a flow
	 */
	Layer* GetPrevLayer() const;

	/**
	 * @brief Get the Next Layer in the flow layer stack
	 *
	 * @return The layer or nullptr if this is the last layer,
	 *         or the layer is not associated with a flow
	 */
	Layer* GetNextLayer() const;

	/**
	 * @brief Check if the layer _is of_ or _derives from_ the specified type
	 */
	template <typename T>
	bool Is() const
	{
		return dynamic_cast<const T*>(this) != nullptr;
	}

	/**
	 * @brief The destructor
	 */
	virtual ~Layer() {}

protected:
	/**
	 * @brief Base layer is never constructed directly
	 */
	Layer() {}

	/**
	 * @brief Get the Flow this layer belongs to
	 */
	Flow* GetFlow() const { return _flow; }

	/**
	 * @brief Get the layer number (index of the layer) in the Flow's layer stack
	 */
	size_t GetLayerNumber() const { return _layerNumber; };

	/**
	 * @brief Check if this layer has been added to the provided packet
	 *
	 * @param packet   The packet
	 * @return true if it has, else false
	 */
	bool PacketHasLayer(Packet& packet) const;

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
