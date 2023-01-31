/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <vector>

#include <pcapplusplus/IpAddress.h>
#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief IPv4 address range
 */
class IPv4AddressRange {
public:
	/**
	 * @brief Construct a new uninitialized IPv4 Address Range object
	 */
	IPv4AddressRange() {}

	/**
	 * @brief Construct a new IPv4 Address Range object from a yaml node
	 */
	IPv4AddressRange(const YAML::Node& node);

	/**
	 * @brief Get the base address of the range, respectively the prefix
	 */
	pcpp::IPv4Address GetBaseAddr() const { return _baseAddr; }

	/**
	 * @brief Get the length of the prefix, respectively the number of significant bits in the base
	 * address
	 */
	uint8_t GetPrefixLen() const { return _prefixLen; }

private:
	pcpp::IPv4Address _baseAddr {pcpp::IPv4Address::Zero};
	uint8_t _prefixLen = 0;
};

/**
 * @brief A representation of the IPv4 section in the yaml config
 */
class IPv4 {
public:
	/**
	 * @brief Construct a new IPv4 configuration object
	 */
	IPv4() {}

	/**
	 * @brief Construct a new IPv4 configuration object from a yaml node
	 */
	IPv4(const YAML::Node& node);

	/**
	 * @brief Get the IPv4 address ranges
	 */
	const std::vector<IPv4AddressRange>& GetIpRange() const { return _ipRange; }

	/**
	 * @brief Get the IPv4 fragmentation probability
	 *
	 * @return The probability in the range <0.0, 1.0>
	 */
	double GetFragmentationProbability() const { return _fragmentationProbability; }

	/**
	 * @brief Get the minimal size of a packet to be considered for fragmentation
	 *
	 * @return The size as a value in range <0, 65535>
	 */
	uint16_t GetMinPacketSizeToFragment() const { return _minPacketSizeToFragment; }

private:
	std::vector<IPv4AddressRange> _ipRange;
	double _fragmentationProbability = 0.0;
	uint16_t _minPacketSizeToFragment = 512;
};

} // namespace config
} // namespace generator
