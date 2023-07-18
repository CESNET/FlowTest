/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetsizegenerator.h"
#include "packetsizegeneratorfast.h"
#include "packetsizegeneratorslow.h"

namespace generator {

/**
 * The packet count threshold for deciding between the different variants of generators.
 * Arbitratily chosen. Can be adjusted later.
 */
static constexpr int SLOW_GENERATOR_PKT_COUNT_THRESHOLD = 50;

std::unique_ptr<PacketSizeGenerator> PacketSizeGenerator::Construct(
	const std::vector<IntervalInfo>& intervals,
	uint64_t numPkts,
	uint64_t numBytes)
{
	/**
	 * The "Slow" generator produces better results for smaller flows, but is
	 * expensive to compute. Using this generator for large flows is very slow!
	 *
	 * The "Fast" generator produces is ideal for flows with larger amounts of
	 * packets, as it produces even better results than the "Slow" variant
	 * given enough packets, and is much faster.
	 */
	if (numPkts < SLOW_GENERATOR_PKT_COUNT_THRESHOLD) {
		return std::make_unique<PacketSizeGeneratorSlow>(intervals, numPkts, numBytes);
	} else {
		return std::make_unique<PacketSizeGeneratorFast>(intervals, numPkts, numBytes);
	}
}

} // namespace generator
