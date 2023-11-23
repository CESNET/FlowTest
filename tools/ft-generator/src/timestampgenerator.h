/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Timestamp generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "randomgenerator.h"
#include "timestamp.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace generator {

/**
 * @brief Generate packet timestamps
 *
 * @param numPackets  The number of packets
 * @param tsFirst  Timestamp of the first packet
 * @param tsLast  Timestamp of the last packet
 * @param maxGapSec  The maximum gap between two packets or std::nullopt for unlimited
 * @return A vector of timestamps in microseconds
 *
 * @throws std::logic_error if invalid information about the first and last timestamp is provided
 * @throws std::overflow_error if conversion of maxGapSec to microseconds would overflow uint64
 *
 * If the maximum gap limit cannot be satisfied with the parameters provided, the flow is trimmed
 * such that the number of packets and first timestamp stays the same, but the last timestamp is
 * moved closer to start.
 */
std::vector<uint64_t> GenerateTimestamps(
	uint64_t numPackets,
	const Timestamp& tsFirst,
	const Timestamp& tsLast,
	std::optional<uint64_t> maxGapSec = std::nullopt);

} // namespace generator
