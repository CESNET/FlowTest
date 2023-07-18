/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetsizegenerator.h"
#include "packetsizegeneratorslow.h"

namespace generator {

std::unique_ptr<PacketSizeGenerator> PacketSizeGenerator::Construct(
	const std::vector<IntervalInfo>& intervals,
	uint64_t numPkts,
	uint64_t numBytes)
{
	return std::make_unique<PacketSizeGeneratorSlow>(intervals, numPkts, numBytes);
}

} // namespace generator
