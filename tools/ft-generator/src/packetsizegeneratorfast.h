/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "packetsizegenerator.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace generator {

/**
 * @brief Value generator class
 *
 */
class PacketSizeGeneratorFast : public PacketSizeGenerator {
public:
	/**
	 * @brief Construct a new Packet Size Generator object
	 *
	 * @param intervals   The intervals to generate the values from. The interval ranges must be
	 * continous!
	 * @param numPkts     The target number of packets
	 * @param numBytes    The target number of bytes
	 *
	 * This is not exact, the values provided are mere targets that we will try to approach. We will
	 * likely not reach exactly the specified values, but we will try to do the best we can.
	 */
	PacketSizeGeneratorFast(
		const std::vector<IntervalInfo>& intervals,
		uint64_t numPkts,
		uint64_t numBytes);

	/**
	 * @brief The destructor
	 */
	~PacketSizeGeneratorFast() = default;

	/**
	 * @brief Get an exact value
	 *
	 * @param value  The required value
	 * @return The value
	 */
	uint64_t GetValueExact(uint64_t value) override;

	/**
	 * @brief Get a random value from the generated values
	 *
	 * @return A value
	 */
	uint64_t GetValue() override;

	/**
	 * @brief Plan the remaining number of bytes and packets
	 */
	void PlanRemaining() override;

	/**
	 * @brief Log statistics about generated values
	 */
	void PrintReport() override;

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PacketSizeGeneratorFast");

	std::vector<IntervalInfo> _intervals;
	uint64_t _assignedPkts = 0; //< Number of assigned packets
	uint64_t _assignedBytes = 0; //< Number of assigned bytes
	uint64_t _numPkts; //< The target number of packets
	uint64_t _numBytes; //< The target number of bytes
	std::vector<uint64_t> _availCount; //< How much values are available in individual intervals
	std::vector<uint64_t>
		_assignedCount; //< How much values have been assigned from individual intervals
};

} // namespace generator
