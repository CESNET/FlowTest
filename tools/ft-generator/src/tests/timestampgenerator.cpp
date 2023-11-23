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

static constexpr uint64_t NSEC_IN_SEC = 1'000'000'000;

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
	const auto& timestamps = GenerateTimestamps(
		2,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(500));
	CHECK(timestamps.size() == 2);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(500).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 1);
	CHECK(gaps[0] == 500 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - no max gap limit")
{
	const auto& timestamps = GenerateTimestamps(
		10,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(500));
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(500).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(SumAll(gaps) == 500 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case")
{
	const auto& timestamps = GenerateTimestamps(
		10,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(50),
		10);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(50).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 10 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 50 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case 2")
{
	const auto& timestamps = GenerateTimestamps(
		1000,
		Timestamp::From<TimeUnit::Seconds>(10000),
		Timestamp::From<TimeUnit::Seconds>(20000),
		20);
	CHECK(timestamps.size() == 1000);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(10000).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(20000).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 999);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 20 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10000 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - extreme case")
{
	const auto& timestamps = GenerateTimestamps(
		11,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(10),
		1);
	CHECK(timestamps.size() == 11);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(10).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 10);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - empty case")
{
	const auto& timestamps = GenerateTimestamps(
		0,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(100),
		1);
	CHECK(timestamps.size() == 0);

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.empty());
}

TEST_CASE("timestamp gaps are generated correctly - single packet")
{
	const auto& timestamps = GenerateTimestamps(
		1,
		Timestamp::From<TimeUnit::Seconds>(100),
		Timestamp::From<TimeUnit::Seconds>(100),
		1);
	CHECK(timestamps.size() == 1);
	CHECK(timestamps[0] == Timestamp::From<TimeUnit::Seconds>(100).ToNanoseconds());
}

TEST_CASE("timestamp gaps are generated correctly - trim case")
{
	const auto& timestamps = GenerateTimestamps(
		10,
		Timestamp::From<TimeUnit::Seconds>(10),
		Timestamp::From<TimeUnit::Seconds>(100),
		1);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(10).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(19).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 9 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - trim case 2")
{
	const auto& timestamps = GenerateTimestamps(
		10,
		Timestamp::From<TimeUnit::Seconds>(0),
		Timestamp::From<TimeUnit::Seconds>(60),
		2);
	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == Timestamp::From<TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == Timestamp::From<TimeUnit::Seconds>(18).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 2 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 18 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - invalid case")
{
	CHECK_THROWS_AS(
		GenerateTimestamps(
			10,
			Timestamp::From<TimeUnit::Seconds>(100),
			Timestamp::From<TimeUnit::Seconds>(0),
			1),
		std::logic_error);
}

TEST_SUITE_END();
