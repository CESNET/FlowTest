/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Timestamps configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <limits>

namespace generator {
namespace config {

/**
 * @brief Configuration section for timestamp generation
 */
class Timestamps {
public:
	/**
	 * @brief Construct timestamp configuration with default values
	 */
	Timestamps() = default;

	/**
	 * @brief Construct timestamp configuration from a yaml node
	 */
	Timestamps(const YAML::Node& node);

	/**
	 * @brief Get the link speed value in bytes per second
	 */
	uint64_t GetLinkSpeedBps() const { return _linkSpeedBps; }

	/**
	 * @brief Get the minimal packet gap value in nanoseconds
	 */
	uint64_t GetMinPacketGapNanos() const { return _minPacketGapNanos; }

	/**
	 * @brief Get the minimal gap after switching directions in nanoseconds
	 */
	uint64_t GetFlowMinDirSwitchGapNanos() const { return _flowMinDirSwitchGapNanos; }

	/**
	 * @brief Get maximal gap between two packets of the same flow in nanoseconds
	 */
	uint64_t GetFlowMaxInterpacketGapNanos() const { return _flowMaxInterpacketGapNanos; }

private:
	uint64_t _linkSpeedBps = 100'000'000'000; // 100 Gbps
	uint64_t _minPacketGapNanos = 0;
	uint64_t _flowMinDirSwitchGapNanos = 0;
	uint64_t _flowMaxInterpacketGapNanos = std::numeric_limits<uint64_t>::max(); // No limit
};

} // namespace config
} // namespace generator
