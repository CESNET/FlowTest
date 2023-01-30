/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A flow calendar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "calendar.h"
#include "flow.h"

#include <queue>

namespace generator {

static bool CompareFlows(const std::unique_ptr<Flow>& lhs, const std::unique_ptr<Flow>& rhs)
{
	return lhs->GetNextPacketTime() > rhs->GetNextPacketTime();
}

void Calendar::Push(std::unique_ptr<Flow> flow)
{
	_flows.push_back(std::move(flow));
	std::push_heap(_flows.begin(), _flows.end(), CompareFlows);
}

const Flow& Calendar::Top() const
{
	return *_flows.front().get();
}

std::unique_ptr<Flow> Calendar::Pop()
{
	std::pop_heap(_flows.begin(), _flows.end(), CompareFlows);
	std::unique_ptr<Flow> flow = std::move(_flows.back());
	_flows.pop_back();
	return flow;
}

bool Calendar::IsEmpty() const
{
	return _flows.empty();
}

} // namespace generator
