/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Port range
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <cstdint>

namespace generator {
namespace config {

/**
 * @brief Port range representation
 */
class PortRange {
public:
	/**
	 * @brief Construct PortRange from a YAML node
	 *
	 * @param node The node
	 */
	PortRange(const YAML::Node& node);

	/**
	 * @brief Construct port range given lower and upper bound
	 *
	 * @param from The lower bound
	 * @param to The upper bound (inclusive)
	 */
	PortRange(uint16_t from, uint16_t to);

	/**
	 * @brief Construct port range of a single value
	 *
	 * @param value The value
	 */
	PortRange(uint16_t value);

	/**
	 * @brief Check if port range includes a value
	 *
	 * @param value The value
	 * @return true or false
	 */
	bool Includes(uint16_t value) const;

private:
	uint16_t _from;
	uint16_t _to;
};

} // namespace config
} // namespace generator
