/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Common helper functions used in the the project.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
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
	/** Minimum relative size of the wanted profile sample. */
	double minSampleSize {};
	/** Maximum relative size of the wanted profile sample. */
	double maxSampleSize {};
	/** Relative number of mutating genes (beginning). */
	double mutationHigh {0.005};
	/** Relative number of mutating genes (end). */
	double mutationLow {0.0002};
	/** Omit protocols which proportional representation in the profile is less than a threshold.*/
	double protoThreshold {0.005};
	/** Omit ports which proportional representation in the profile is less than a threshold.*/
	double portThreshold {0.005};
	/** Fitness value to switch between low and high mutation. */
	double mutationCtrl {80};
	/** Print debug messages. */
	bool verbose {true};
};
