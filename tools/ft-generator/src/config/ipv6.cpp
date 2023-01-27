/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "ipv6.h"

namespace generator {
namespace config {

IPv6AddressRange::IPv6AddressRange(const YAML::Node& node)
{
	auto pieces = stringSplit(asScalar(node), "/");
	if (pieces.size() != 2) {
		throw ConfigError(node, "invalid IPv6 range");
	}

	auto prefixLen = parseValue<uint8_t>(pieces[1]);
	if (!prefixLen || *prefixLen > 128) {
		throw ConfigError(node, "invalid IPv6 range prefix");
	}
	_prefixLen = *prefixLen;

	_baseAddr = pcpp::IPv6Address(pieces[0]);
	if (!_baseAddr.isValid()) {
		throw ConfigError(node, "invalid IPv6 range address");
	}
}

IPv6::IPv6(const YAML::Node& node)
{
	checkAllowedKeys(node, {
		"ip_range",
	});
	_ipRange = parseOneOrMany<IPv6AddressRange>(node["ip_range"]);
}

} // namespace config
} // namespace generator
