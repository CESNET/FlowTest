/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

#include <pcapplusplus/MacAddress.h>
#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief Mac address range
 *
 */
class MacAddressRange {
public:
	/**
	 * @brief Construct a new uninitialized Mac Address Range object
	 *
	 */
	MacAddressRange() {}

	/**
	 * @brief Construct a new Mac Address Range object from a yaml node
	 *
	 * @param node
	 */
	MacAddressRange(const YAML::Node& node);

	/**
	 * @brief Get the base address of the range, respectively the prefix
	 *
	 * @return pcpp::MacAddress
	 */
	pcpp::MacAddress GetBaseAddr() const { return _baseAddr; };

	/**
	 * @brief Get the length of the prefix, respectively the number of significant bits in the base
	 * address
	 *
	 * @return uint8_t
	 */
	uint8_t GetPrefixLen() const { return _prefixLen; };

	/**
	 * @brief Compare the mac address range for equality
	 *
	 * @param other The other range
	 * @return true if the ranges match, else false
	 */
	bool operator==(const MacAddressRange& other) const;

private:
	pcpp::MacAddress _baseAddr {pcpp::MacAddress::Zero};
	uint8_t _prefixLen;
};

/**
 * @brief A representation of the Mac section in the yaml config
 *
 */
class Mac {
public:
	/**
	 * @brief Construct a new Mac configuration object
	 *
	 */
	Mac() {}

	/**
	 * @brief Construct a new Mac configuration object from a yaml node
	 *
	 * @param node
	 */
	Mac(const YAML::Node& node);

	/**
	 * @brief Get the Mac address range
	 *
	 * @return const std::vector<MacAddressRange>&
	 */
	const std::vector<MacAddressRange>& GetMacRange() const { return _macRange; }

private:
	std::vector<MacAddressRange> _macRange;
};

} // namespace config
} // namespace generator
