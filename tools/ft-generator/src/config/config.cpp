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

	if (node["ipv4"].IsDefined()) {
		_ipv4 = IPv4(node["ipv4"]);
	}

	if (node["ipv6"].IsDefined()) {
		_ipv6 = IPv6(node["ipv6"]);
	}

	if (node["mac"].IsDefined()) {
		_mac = Mac(node["mac"]);
	}

	if (node["encapsulation"].IsDefined()) {
		_encapsulation = Encapsulation(node["encapsulation"]);
	}
}

Config Config::LoadFromFile(const std::string& configFilename)
{
	YAML::Node node = YAML::LoadFile(configFilename);
	return Config(node);
}

} // namespace config
} // namespace generator
