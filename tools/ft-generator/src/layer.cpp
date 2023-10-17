/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Layer planner interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "layer.h"
#include "flow.h"

#include <stdexcept>

namespace generator {

void Layer::AddedToFlow(Flow* flow, size_t layerNumber)
{
	_flow = flow;
	_layerNumber = layerNumber;
}

Layer* Layer::GetPrevLayer() const
{
	if (!_flow || _layerNumber == 0) {
		return nullptr;
	}
	return _flow->_layerStack[_layerNumber - 1].get();
}

Layer* Layer::GetNextLayer() const
{
	if (!_flow || _layerNumber >= _flow->_layerStack.size() - 1) {
		return nullptr;
	}
	return _flow->_layerStack[_layerNumber + 1].get();
}

bool Layer::PacketHasLayer(Packet& packet) const
{
	return _layerNumber < packet._layers.size();
}

Packet::layerParams& Layer::GetPacketParams(Packet& packet) const
{
	assert(PacketHasLayer(packet));
	return packet._layers[_layerNumber].second;
}

void Layer::PostPlanFlow(Flow& flow)
{
	(void) flow;
}

void Layer::PlanExtra(Flow& flow)
{
	(void) flow;
}

void Layer::PostBuild(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	(void) packet;
	(void) params;
	(void) plan;
}

size_t Layer::SizeUpToIpLayer(Packet& packet) const
{
	(void) packet;
	throw std::logic_error("unimplemented method");
}

} // namespace generator
