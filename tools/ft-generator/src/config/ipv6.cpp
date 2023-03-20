/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv6.h"
#include "common.h"

namespace generator {
namespace config {

IPv6AddressRange::IPv6AddressRange(const YAML::Node& node)
{
	auto pieces = StringSplit(AsScalar(node), "/");
	if (pieces.size() != 2) {
		throw ConfigError(node, "invalid IPv6 range");
	}

	auto prefixLen = ParseValue<uint8_t>(pieces[1]);
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
	CheckAllowedKeys(
		node,
		{"ip_range", "fragmentation_probability", "min_packet_size_to_fragment"});

	const auto& ipRangeNode = node["ip_range"];
	if (ipRangeNode.IsDefined()) {
		_ipRange = ParseOneOrMany<IPv6AddressRange>(ipRangeNode);
	}

	const auto& fragProbNode = node["fragmentation_probability"];
	if (fragProbNode.IsDefined()) {
		_fragmentationProbability = ParseProbability(fragProbNode);
	}

	const auto& minPktSizeToFragmentNode = node["min_packet_size_to_fragment"];
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
