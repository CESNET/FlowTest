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
		{"ipv4",
		 "ipv6",
		 "mac",
		 "encapsulation",
		 "packet_size_probabilities",
		 "max_flow_inter_packet_gap"});

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

	if (node["packet_size_probabilities"].IsDefined()) {
		_packetSizeProbabilities = PacketSizeProbabilities(node["packet_size_probabilities"]);
	}

	const auto& maxFlowInterPacketGapNode = node["max_flow_inter_packet_gap"];
	if (maxFlowInterPacketGapNode.IsDefined()) {
		if (maxFlowInterPacketGapNode.IsNull()) {
			_maxFlowInterPacketGapSecs = std::nullopt;
		} else {
			auto value = ParseValue<uint64_t>(AsScalar(maxFlowInterPacketGapNode));
			if (!value || *value == 0) {
				throw ConfigError(
					node,
					"invalid max flow inter packet gap value, expected integer > 0");
			}
			_maxFlowInterPacketGapSecs = *value;
		}
	}
}

Config Config::LoadFromFile(const std::string& configFilename)
{
	YAML::Node node;
	try {
		node = YAML::LoadFile(configFilename);
	} catch (const YAML::BadFile& ex) {
		throw std::runtime_error("Config file \"" + configFilename + "\" cannot be loaded");
	}
	return Config(node);
}

} // namespace config
} // namespace generator
