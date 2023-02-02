/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The main yaml configuration object
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

namespace generator {
namespace config {

Config::Config(const YAML::Node& node)
{
	CheckAllowedKeys(
		node,
		{
			"ipv4",
			"ipv6",
			"mac",
			"encapsulation",
		});
	_ipv4 = IPv4(node["ipv4"]);
	_ipv6 = IPv6(node["ipv6"]);
	_mac = Mac(node["mac"]);
	_encapsulation = Encapsulation(node["encapsulation"]);
}

Config Config::LoadFromFile(const std::string& configFilename)
{
	YAML::Node node = YAML::LoadFile(configFilename);
	return Config(node);
}

} // namespace config
} // namespace generator
