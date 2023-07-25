/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include <memory>
#include <vector>

namespace generator {

/**
 * @brief Definition of a probability interval
 */
struct IntervalInfo {
	uint64_t _from; //< Left bound of the interval
	uint64_t _to; //< Right bound of the interval
	float _probability; //< Probability of choosing this interval
};

/**
 * @brief Value generator class
 *
 */
class PacketSizeGenerator {
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
	PacketSizeGenerator(
		const std::vector<IntervalInfo>& intervals,
		uint64_t numPkts,
		uint64_t numBytes);

	/**
	 * @brief The destructor
	 */
	~PacketSizeGenerator() = default;

	/**
	 * @brief Get an exact value
	 *
	 * @param value  The required value
	 * @return The value
	 */
	uint64_t GetValueExact(uint64_t value);

	/**
	 * @brief Get a random value from the generated values
	 *
	 * @return A value
	 */
	uint64_t GetValue();

	/**
	 * @brief Plan the remaining number of bytes and packets
	 */
	void PlanRemaining();

	/**
	 * @brief Log statistics about generated values
	 */
	void PrintReport();

private:
	std::vector<IntervalInfo> _intervals;
	uint64_t _numPkts;
	uint64_t _numBytes;

	uint64_t _assignedPkts = 0;
	uint64_t _assignedBytes = 0;

	std::vector<uint64_t> _values;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PacketSizeGenerator");

	void Generate(uint64_t desiredPkts, uint64_t desiredBytes);
};

} // namespace generator
