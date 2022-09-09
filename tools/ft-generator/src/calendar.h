/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A flow calendar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>
#include <memory>
#include <limits>

namespace generator {

class Flow;

/**
 * @brief A simulation calendar for flow event planning
 *
 * @note We cannot simply use std::queue as it does not support peforming std::move on the top as it's popped
 */
class Calendar {
public:
	/**
	 * @brief Add a flow to the calendar
	 *
	 * @param flow  The flow
	 */
	void Push(std::unique_ptr<Flow> flow);

	/**
	 * @brief Get reference to the flow at the top of the calendar
	 *
	 * @return The flow
	 */
	const Flow& Top() const;

	/**
	 * @brief Retrieve the flow at the top of the calendar
	 *
	 * @return The flow
	 */
	std::unique_ptr<Flow> Pop();

	/**
	 * @brief Check whether the calendar is empty
	 *
	 * @return true or false
	 */
	bool IsEmpty() const;

private:
	std::vector<std::unique_ptr<Flow>> _flows;
};

} // namespace generator
