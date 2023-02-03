/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv4.h"
#include "common.h"

namespace generator {
namespace config {

IPv4AddressRange::IPv4AddressRange(const YAML::Node& node)
{
	auto pieces = StringSplit(AsScalar(node), "/");
	if (pieces.size() != 2) {
		throw ConfigError(node, "invalid IPv4 range");
	}

	auto prefixLen = ParseValue<uint8_t>(pieces[1]);
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
	CheckAllowedKeys(
		node,
		{"ip_range", "fragmentation_probability", "min_packet_size_to_fragment"});

	_ipRange = ParseOneOrMany<IPv4AddressRange>(node["ip_range"]);
	_fragmentationProbability = ParseProbability(node["fragmentation_probability"]);

	auto minPktSizeToFragmentNode = node["min_packet_size_to_fragment"];
	if (minPktSizeToFragmentNode.IsDefined()) {
		auto result = ParseValue<uint16_t>(AsScalar(minPktSizeToFragmentNode));
		if (!result) {
			throw ConfigError(minPktSizeToFragmentNode, "expected integer in range <0, 65535>");
		}
		_minPacketSizeToFragment = *result;
	}
}

} // namespace config
} // namespace generator
