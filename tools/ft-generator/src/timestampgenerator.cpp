/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Timestamp generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "timestampgenerator.h"
#include "randomgenerator.h"
#include "timestamp.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace generator {

static constexpr uint64_t NANOSEC_IN_SEC = 1'000'000'000;

static std::vector<uint64_t> GenerateRandomGaps(uint64_t numPackets, uint64_t flowDurationNsec)
{
	auto& rng = RandomGenerator::GetInstance();
	std::vector<double> tNorm(numPackets);
	tNorm.front() = 0.0;
	tNorm.back() = 1.0;
	for (std::size_t i = 1; i < tNorm.size() - 1; i++) {
		tNorm[i] = rng.RandomDouble();
	}
	std::sort(tNorm.begin(), tNorm.end());

	std::vector<uint64_t> gaps(numPackets - 1);
	uint64_t sum = 0;
	for (std::size_t i = 0; i < tNorm.size() - 1; i++) {
		uint64_t value = (tNorm[i + 1] - tNorm[i]) * flowDurationNsec;
		sum += value;
		gaps[i] = value;
	}

	gaps.front() += flowDurationNsec - sum; // Compensate for rounding error

	return gaps;
}

static void Redistribute(
	uint64_t amountToRedistribute,
	uint64_t limitPerValue,
	std::vector<uint64_t>& values,
	std::size_t& partitionBoundary)
{
	while (amountToRedistribute > 0 && partitionBoundary > 0) {
		std::size_t i = RandomGenerator::GetInstance().RandomUInt(0, partitionBoundary - 1);

		uint64_t amountToAdd = std::min(limitPerValue - values[i], amountToRedistribute);
		values[i] += amountToAdd;
		amountToRedistribute -= amountToAdd;

		if (values[i] == limitPerValue) {
			std::swap(values[i], values[partitionBoundary - 1]);
			partitionBoundary--;
		}
	}
}

static void ApplyLimit(uint64_t limitPerValue, std::vector<uint64_t>& values)
{
	auto boundary = std::partition(values.begin(), values.end(), [=](uint64_t v) {
		return v < limitPerValue;
	});
	std::size_t boundaryIndex = std::distance(values.begin(), boundary);

	for (std::size_t i = boundaryIndex; i < values.size(); i++) {
		Redistribute(values[i] - limitPerValue, limitPerValue, values, boundaryIndex);
		values[i] = limitPerValue;
	}
}

static void GapsToTimestampsInPlace(uint64_t startUsec, std::vector<uint64_t>& values)
{
	uint64_t t = startUsec;
	values.resize(values.size() + 1);
	for (std::size_t i = 0; i < values.size() - 1; i++) {
		uint64_t gap = values[i];
		values[i] = t;
		t += gap;
	}
	values.back() = t;
}

std::vector<uint64_t> GenerateTimestamps(
	uint64_t numPackets,
	const Timestamp& tsFirst,
	const Timestamp& tsLast,
	std::optional<uint64_t> maxGapSec)
{
	assert(tsFirst >= Timestamp::From<TimeUnit::Seconds>(0));
	assert(tsLast >= Timestamp::From<TimeUnit::Seconds>(0));

	// Prepare and check arguments and handle special cases
	uint64_t start = tsFirst.ToNanoseconds();
	uint64_t end = tsLast.ToNanoseconds();
	uint64_t limit = maxGapSec ? OverflowCheckedMultiply(*maxGapSec, NANOSEC_IN_SEC)
							   : std::numeric_limits<uint64_t>::max();

	if (start > end) {
		throw std::logic_error("start timestamp > end timestamp");
	}

	if (numPackets == 0) {
		return {};
	} else if (numPackets == 1) {
		if (start != end) {
			throw std::logic_error("flow has one packet and start timestamp != end timestamp");
		}
		return {start};
	}

	// Generate the timestamps
	std::vector<uint64_t> values = GenerateRandomGaps(numPackets, end - start);

	ApplyLimit(limit, values);

	RandomGenerator::GetInstance().Shuffle(values);

	GapsToTimestampsInPlace(start, values);

	return values;
}
} // namespace generator
