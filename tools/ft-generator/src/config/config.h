/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The main yaml configuration object
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "common.h"

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
	 * @brief Load a configuration from a yaml file
	 *
	 * @param configFilename  Path to the yaml file
	 * @return Config         The constructed configuration
	 */
	static Config LoadFromFile(const std::string& configFilename);

private:
};

} // namespace config
} // namespace generator
