/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mac.h"
#include "../utils.h"
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

	// Zero out the non-prefix bits
	uint8_t bytes[6];
	_baseAddr.copyTo(bytes);
	for (uint8_t i = _prefixLen; i < 48; i++) {
		SetBit(i, 0, bytes);
	}
	_baseAddr = pcpp::MacAddress(bytes);
}

bool MacAddressRange::operator==(const MacAddressRange& other) const
{
	return _baseAddr == other._baseAddr && _prefixLen == other._prefixLen;
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
