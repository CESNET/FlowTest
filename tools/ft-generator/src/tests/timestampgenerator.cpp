/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for flow.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../timestampgenerator.h"

#include <doctest.h>

#include <algorithm>
#include <numeric>

using namespace generator;

static constexpr uint64_t USEC_IN_SEC = 1'000'000;

static uint64_t SumAll(const std::vector<uint64_t>& values)
{
	return std::accumulate(values.begin(), values.end(), uint64_t(0));
}

static std::vector<uint64_t> GetGaps(const std::vector<uint64_t>& timestamps)
{
	if (timestamps.size() <= 1) {
		return {};
	}

	std::vector<uint64_t> gaps;
	gaps.reserve(timestamps.size() - 1);
	for (std::size_t i = 1; i < timestamps.size(); i++) {
		gaps.push_back(timestamps[i] - timestamps[i - 1]);
	}
	return gaps;
}

TEST_SUITE_BEGIN("TimestampGenerator");

TEST_CASE("timestamp gaps are generated correctly - single value and no max gap limit")
{
	const auto& timestamps = GenerateTimestamps(2, Timeval(0, 0), Timeval(500, 0));
	CHECK(timestamps.size() == 2);
	CHECK(timestamps.front() == Timeval(0, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(500, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 1);
	CHECK(gaps[0] == 500 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - no max gap limit")
{
	const auto& timestamps = GenerateTimestamps(10, Timeval(0, 0), Timeval(500, 0));
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timeval(0, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(500, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(SumAll(gaps) == 500 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case")
{
	const auto& timestamps = GenerateTimestamps(10, Timeval(0, 0), Timeval(50, 0), 10);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timeval(0, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(50, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 10 * USEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 50 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case 2")
{
	const auto& timestamps = GenerateTimestamps(1000, Timeval(10000, 0), Timeval(20000, 0), 20);
	CHECK(timestamps.size() == 1000);
	CHECK(timestamps.front() == Timeval(10000, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(20000, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 999);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 20 * USEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10000 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - extreme case")
{
	const auto& timestamps = GenerateTimestamps(11, Timeval(0, 0), Timeval(10, 0), 1);
	CHECK(timestamps.size() == 11);
	CHECK(timestamps.front() == Timeval(0, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(10, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 10);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * USEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - empty case")
{
	const auto& timestamps = GenerateTimestamps(0, Timeval(0, 0), Timeval(100, 0), 1);
	CHECK(timestamps.size() == 0);

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.empty());
}

TEST_CASE("timestamp gaps are generated correctly - single packet")
{
	const auto& timestamps = GenerateTimestamps(1, Timeval(100, 0), Timeval(100, 0), 1);
	CHECK(timestamps.size() == 1);
	CHECK(timestamps[0] == Timeval(100, 0).ToMicroseconds());
}

TEST_CASE("timestamp gaps are generated correctly - trim case")
{
	const auto& timestamps = GenerateTimestamps(10, Timeval(10, 0), Timeval(100, 0), 1);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timeval(10, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(19, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * USEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 9 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - trim case 2")
{
	const auto& timestamps = GenerateTimestamps(10, Timeval(0, 0), Timeval(60, 0), 2);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timeval(0, 0).ToMicroseconds());
	CHECK(timestamps.back() == Timeval(18, 0).ToMicroseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 2 * USEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 18 * USEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - invalid case")
{
	CHECK_THROWS_AS(GenerateTimestamps(10, Timeval(100, 0), Timeval(0, 0), 1), std::logic_error);
}

TEST_SUITE_END();
