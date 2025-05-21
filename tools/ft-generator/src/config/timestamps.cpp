/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Timestamps configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "timestamps.h"
#include "common.h"

namespace generator {
namespace config {

Timestamps::Timestamps(const YAML::Node& node)
{
	CheckAllowedKeys(
		node,
		{
			"link_speed",
			"min_packet_gap",
			"flow_min_dir_switch_gap",
			"flow_max_interpacket_gap",
		});

	if (node["link_speed"].IsDefined()) {
		_linkSpeedBps = ParseSpeedUnit(node["link_speed"]);
	}

	if (node["min_packet_gap"].IsDefined()) {
		_minPacketGapNanos = ParseTimeUnit(node["min_packet_gap"]);
	}

	if (node["flow_min_dir_switch_gap"].IsDefined()) {
		_flowMinDirSwitchGapNanos = ParseTimeUnit(node["flow_min_dir_switch_gap"]);
	}

	if (node["flow_max_interpacket_gap"].IsDefined()) {
		_flowMaxInterpacketGapNanos = ParseTimeUnit(node["flow_max_interpacket_gap"]);
	}
}

} // namespace config
} // namespace generator
