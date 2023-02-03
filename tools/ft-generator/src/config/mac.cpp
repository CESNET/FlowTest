/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mac.h"
#include "common.h"

namespace generator {
namespace config {

MacAddressRange::MacAddressRange(const YAML::Node& node)
{
	auto pieces = StringSplit(AsScalar(node), "/");
	if (pieces.size() != 2) {
		throw ConfigError(node, "invalid mac range");
	}

	auto prefixLen = ParseValue<uint8_t>(pieces[1]);
	if (!prefixLen || *prefixLen > 48) {
		throw ConfigError(node, "invalid mac range prefix");
	}
	_prefixLen = *prefixLen;

	_baseAddr = pcpp::MacAddress(pieces[0]);
	if (!_baseAddr.isValid()) {
		throw ConfigError(node, "invalid mac range address");
	}
}

Mac::Mac(const YAML::Node& node)
{
	CheckAllowedKeys(
		node,
		{
			"mac_range",
		});
	_macRange = ParseOneOrMany<MacAddressRange>(node["mac_range"]);
}

} // namespace config
} // namespace generator
