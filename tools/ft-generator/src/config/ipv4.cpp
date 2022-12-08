/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "ipv4.h"

namespace generator {
namespace config {

IPv4AddressRange::IPv4AddressRange(const YAML::Node& node)
{
	auto pieces = stringSplit(asScalar(node), "/");
	if (pieces.size() != 2) {
		throw ConfigError(node, "invalid IPv4 range");
	}

	auto prefixLen = parseValue<uint8_t>(pieces[1]);
	if (!prefixLen || *prefixLen > 32) {
		throw ConfigError(node, "invalid IPv4 range prefix");
	}
	_prefixLen = *prefixLen;

	_baseAddr = pcpp::IPv4Address(pieces[0]);
	if (!_baseAddr.isValid()) {
		throw ConfigError(node, "invalid IPv4 range address");
	}
}

IPv4::IPv4(const YAML::Node& node)
{
	checkAllowedKeys(node, {
		"ip_range",
	});
	_ipRange = parseOneOrMany<IPv4AddressRange>(node["ip_range"]);
}

} // namespace config
} // namespace generator
