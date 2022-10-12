/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The main yaml configuration object
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "common.h"
#include "ipv4.h"
#include "ipv6.h"
#include "mac.h"

#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief The top level yaml configuration object
 *
 */
class Config {
public:
	/**
	 * @brief Construct default config
	 *
	 */
	Config() {}

	/**
	 * @brief Construct a config from a yaml node
	 *
	 * @param node
	 *
	 * @throw ConfigError on error in the configuration
	 */
	Config(const YAML::Node& node);

	/**
	 * @brief Get the ipv4 configuration
	 *
	 * @return const IPv4&
	 */
	const IPv4& GetIPv4() const { return _ipv4; }

	/**
	 * @brief Get the ipv6 configuration
	 *
	 * @return const IPv6&
	 */
	const IPv6& GetIPv6() const { return _ipv6; }

	/**
	 * @brief Get the mac configuration
	 *
	 * @return const Mac&
	 */
	const Mac& GetMac() const { return _mac; }

	/**
	 * @brief Load a configuration from a yaml file
	 *
	 * @param configFilename  Path to the yaml file
	 * @return Config         The constructed configuration
	 */
	static Config LoadFromFile(const std::string& configFilename);

private:
	IPv4 _ipv4;
	IPv6 _ipv6;
	Mac _mac;
};

} // namespace config
} // namespace generator
