/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>
#include <memory>

namespace generator {

/**
 * @brief Definition of an interval
 *
 */
struct IntervalInfo {
	uint64_t _from;     //< Left bound of the interval
	uint64_t _to;       //< Right bound of the interval
	float _probability; //< Probability of choosing this interval
};

/**
 * @brief Value generator class
 *
 */
class ValueGenerator {
public:
	/**
	 * @brief Construct a new Value Generator object
	 *
	 * @param count       The total number of generated values
	 * @param desiredSum  The desired number of bytes the values should add up to
	 * @param intervals   The intervals to generate the values from
	 */
	ValueGenerator(uint64_t count, uint64_t desiredSum, const std::vector<IntervalInfo>& intervals);

	/**
	 * @brief Get a random value from the generated values
	 *
	 * @return A value
	 */
	uint64_t GetValue();

	/**
	 * @brief Get an exact value
	 *
	 * @param value  The required value
	 * @return The value
	 */
	uint64_t GetValueExact(uint64_t value);

private:
	std::random_device _rd;
	std::mt19937 _gen{_rd()};
	std::uniform_real_distribution<> _distr;

	std::vector<uint64_t> _values;

	uint64_t _count;
	uint64_t _desiredSum;
	std::vector<IntervalInfo> _intervals;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("ValueGenerator");

	void Generate();
	void PostIntervalUpdate();
	uint64_t GenerateRandomValue();
};

} // namespace generator
