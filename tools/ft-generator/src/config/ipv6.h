/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

#include <pcapplusplus/IpAddress.h>
#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief IPv6 address range
 */
class IPv6AddressRange {
public:
	/**
	 * @brief Construct a new uninitialized IPv6 Address Range object
	 */
	IPv6AddressRange() {}

	/**
	 * @brief Construct a new IPv6 Address Range object from a yaml node
	 */
	IPv6AddressRange(const YAML::Node& node);

	/**
	 * @brief Get the base address of the range, respectively the prefix
	 */
	pcpp::IPv6Address GetBaseAddr() const { return _baseAddr; };

	/**
	 * @brief Get the length of the prefix, respectively the number of significant bits in the base
	 * address
	 */
	uint8_t GetPrefixLen() const { return _prefixLen; };

private:
	pcpp::IPv6Address _baseAddr {pcpp::IPv6Address::Zero};
	uint8_t _prefixLen = 0;
};

/**
 * @brief A representation of the IPv6 section in the yaml config
 *
 */
class IPv6 {
public:
	/**
	 * @brief Construct a new IPv6 configuration object
	 */
	IPv6() {}

	/**
	 * @brief Construct a new IPv6 configuration object from a yaml node
	 */
	IPv6(const YAML::Node& node);

	/**
	 * @brief Get the IPv6 address ranges
	 */
	const std::vector<IPv6AddressRange>& GetIpRange() const { return _ipRange; }

private:
	std::vector<IPv6AddressRange> _ipRange;
};

} // namespace config
} // namespace generator
