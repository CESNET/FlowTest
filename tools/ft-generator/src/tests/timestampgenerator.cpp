/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for flow.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../timestampgenerator.h"
#include "../config/timestamps.h"
#include "../packet.h"

#include <doctest.h>
#include <pcapplusplus/EthLayer.h>
#include <yaml-cpp/yaml.h>

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

static std::list<Packet> MakePackets(std::size_t numPackets)
{
	std::list<Packet> packets;
	Direction dir = Direction::Forward;
	for (std::size_t i = 0; i < numPackets; i++) {
		Packet packet {};
		packet._size = 64;
		packet._direction = dir;
		packets.push_back(packet);
		dir = SwapDirection(dir);
	}
	return packets;
}

static config::Timestamps MakeConfig(const std::string& yaml = "")
{
	auto node = YAML::Load(yaml);
	return config::Timestamps(node);
}

static std::vector<uint64_t> GenerateTimestamps(
	const std::list<Packet>& packets,
	const ft::Timestamp& tsFirst,
	const ft::Timestamp& tsLast,
	const config::Timestamps& config)
{
	TimestampGenerator tsGen(packets, tsFirst, tsLast, config, sizeof(pcpp::ether_header));
	std::vector<uint64_t> timestamps;
	timestamps.resize(packets.size());
	for (auto& ts : timestamps) {
		ts = tsGen.Get().ToNanoseconds();
		tsGen.Next();
	}
	return timestamps;
}

TEST_SUITE_BEGIN("TimestampGenerator");

TEST_CASE("timestamp gaps are generated correctly - single value and no max gap limit")
{
	auto packets = MakePackets(2);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(500),
		config::Timestamps());

	CHECK(timestamps.size() == 2);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(500).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 1);
	CHECK(gaps[0] == 500 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - no max gap limit")
{
	auto packets = MakePackets(10);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(500),
		config::Timestamps());

	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(500).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(SumAll(gaps) == 500 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case")
{
	auto packets = MakePackets(10);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(50),
		MakeConfig("flow_max_interpacket_gap: 10s"));

	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(50).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 10 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 50 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - basic case 2")
{
	auto packets = MakePackets(1000);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(10000),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(20000),
		MakeConfig("flow_max_interpacket_gap: 20s"));

	CHECK(timestamps.size() == 1000);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(10000).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(20000).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 999);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 20 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10000 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - extreme case")
{
	auto packets = MakePackets(11);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(10),
		MakeConfig("flow_max_interpacket_gap: 1s"));

	CHECK(timestamps.size() == 11);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(10).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 10);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 10 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - empty case")
{
	auto packets = MakePackets(0);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(100),
		MakeConfig("flow_max_interpacket_gap: 1s"));

	CHECK(timestamps.size() == 0);

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.empty());
}

TEST_CASE("timestamp gaps are generated correctly - single packet")
{
	auto packets = MakePackets(1);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(100),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(100),
		MakeConfig("flow_max_interpacket_gap: 1s"));

	CHECK(timestamps.size() == 1);
	CHECK(timestamps[0] == ft::Timestamp::From<ft::TimeUnit::Seconds>(100).ToNanoseconds());
}

TEST_CASE("timestamp gaps are generated correctly - trim case")
{
	auto packets = MakePackets(10);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(10),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(100),
		MakeConfig("flow_max_interpacket_gap: 1s"));

	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(10).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(19).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 1 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 9 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - trim case 2")
{
	auto packets = MakePackets(10);
	const auto& timestamps = GenerateTimestamps(
		packets,
		ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
		ft::Timestamp::From<ft::TimeUnit::Seconds>(60),
		MakeConfig("flow_max_interpacket_gap: 2s"));

	CHECK(timestamps.size() == 10);
	CHECK(timestamps.front() == ft::Timestamp::From<ft::TimeUnit::Seconds>(0).ToNanoseconds());
	CHECK(timestamps.back() == ft::Timestamp::From<ft::TimeUnit::Seconds>(18).ToNanoseconds());

	const auto& gaps = GetGaps(timestamps);
	CHECK(gaps.size() == 9);
	CHECK(std::all_of(gaps.begin(), gaps.end(), [](auto v) { return v <= 2 * NSEC_IN_SEC; }));
	CHECK(SumAll(gaps) == 18 * NSEC_IN_SEC);
}

TEST_CASE("timestamp gaps are generated correctly - invalid case")
{
	auto packets = MakePackets(10);
	CHECK_THROWS_AS(
		GenerateTimestamps(
			packets,
			ft::Timestamp::From<ft::TimeUnit::Seconds>(100),
			ft::Timestamp::From<ft::TimeUnit::Seconds>(0),
			config::Timestamps()),
		std::logic_error);
}

TEST_SUITE_END();
