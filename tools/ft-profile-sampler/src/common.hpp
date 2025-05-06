/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Common helper functions used in the the project.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "timestamp.h"
#include <charconv>
#include <cinttypes>
#include <cstring>
#include <iostream>

/**
 * @brief Parse numeric value from a string.
 * @tparam T type of the value
 * @param str beginning of the string
 * @param value argument where the parsed value should be stored
 * @throws runtime_error parsing error
 */
template <typename T>
void FromString(std::string_view str, T& value)
{
	const char* begin = str.data();
	const char* end = begin + str.size();

	auto [ptr, ec] {std::from_chars(begin, end, value)};
	if (ec == std::errc() && ptr == end) {
		return;
	}

	const std::string valueStr {str};

	if (ec == std::errc::result_out_of_range) {
		throw std::runtime_error("'" + valueStr + "' is out of range");
	} else if (ec == std::errc::invalid_argument) {
		throw std::runtime_error("'" + valueStr + "' is not a valid number");
	} else {
		throw std::runtime_error(
			"'" + valueStr + "' is not a valid number due to unexpected characters");
	}
}

/**
 * @brief Specialization of FromString for ft::Timestamp object.
 * 	Input string must represent milliseconds.
 * @param str input string containing timestamp in milliseconds, can be negative
 * @param value argument where the parsed value should be stored
 */
template <>
inline void FromString<ft::Timestamp>(std::string_view str, ft::Timestamp& value)
{
	int64_t tsInt;
	FromString(str, tsInt);

	value = ft::Timestamp::From<ft::TimeUnit::Milliseconds>(tsInt);
}

/**
 * @brief Convert timestamp back to milliseconds.
 * 	Nanoseconds are truncated – profiler assumes millisecond timestamps.
 * @param ts timestamp to convert
 * @return milliseconds from epoch
 */
inline int64_t TimestampToMilliseconds(const ft::Timestamp& ts)
{
	constexpr int divisor = 1'000'000;

	int64_t value = ts.ToNanoseconds();
	return value / divisor;
}

/**
 * @brief Configuration object for the evolution process.
 */
struct EvolutionConfig {
	/**
	 * @brief Default init, seed is created from the current time.
	 */
	EvolutionConfig()
		: seed(time(nullptr))
	{
	}
	friend std::ostream& operator<<(std::ostream& os, const EvolutionConfig& cfg);

	/** Seed to be used for the RNG in the evolution. */
	uint64_t seed;
	/** Maximum number of generations the evolution runs. */
	uint32_t generations {500};
	/** Number of individuals in every generation. */
	uint32_t population {16};
	/** Maximum acceptable deviation of individual metrics (0 - 1). */
	double deviation {0.005};
	/** Minimum relative size of the wanted profile sample (0 - 1). */
	double minSampleSize {};
	/** Maximum relative size of the wanted profile sample (0 - 1). */
	double maxSampleSize {};
	/** Relative number of mutating genes (beginning) (0 - 1). */
	double mutationHigh {0.005};
	/** Relative number of mutating genes (end) (0 - 1). */
	double mutationLow {0.0002};
	/** Omit protocols which proportional representation in the profile is less than a threshold.
	 * (0 - 1) */
	double protoThreshold {0.005};
	/** Omit ports which proportional representation in the profile is less than a threshold.
	 * (0 - 1) */
	double portThreshold {0.005};
	/** Fitness value to switch between low and high mutation. */
	double mutationCtrl {80};
	/** Print debug messages. */
	bool verbose {true};
	/** Length of metrics window in seconds. */
	size_t windowLength {5};
	/** Number of parallel workers (threads) used by evolution algorithm. */
	uint8_t workersCount {8};
};
