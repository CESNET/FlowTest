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
#include <numeric>
#include <vector>

namespace generator {

static constexpr uint64_t ETH_PREAMBLE_SIZE = 8;
static constexpr uint64_t ETH_CRC_SIZE = 4;
static constexpr uint64_t ETH_INTERPACKET_GAP_SIZE = 12;
static constexpr uint64_t ETH_MIN_FRAME_SIZE = 64;

static uint64_t CalcPacketTransferTimePicos(uint64_t packetSize, uint64_t linkSpeedBps)
{
	uint64_t frameWithCrc = std::max<uint64_t>(packetSize + ETH_CRC_SIZE, ETH_MIN_FRAME_SIZE);
	uint64_t frameAligned = (frameWithCrc + 7) / 8 * 8; // Round up to multiple of 8
	uint64_t frameTotal = frameAligned + ETH_PREAMBLE_SIZE + ETH_INTERPACKET_GAP_SIZE;

	uint64_t picos = frameTotal * 1'000'000'000'000 / linkSpeedBps;

	return picos;
}

uint64_t CalcMinGlobalPacketGapPicos(uint64_t packetSize, const config::Timestamps& config)
{
	uint64_t minGap = CalcPacketTransferTimePicos(packetSize, config.GetLinkSpeedBps());
	minGap += config.GetMinPacketGapNanos() * 1000;
	return minGap;
}

static std::vector<Gap> GenerateMinimalGaps(
	const std::list<Packet>& packets,
	const config::Timestamps& config,
	uint64_t sizeTillIpLayer)
{
	std::vector<Gap> gaps;
	gaps.resize(packets.size() - 1);
	auto it = packets.begin();

	for (std::size_t i = 0; i < packets.size() - 1; i++) {
		auto& pkt = *it;
		it++;
		auto& nextPkt = *it;

		uint64_t minGap = CalcMinGlobalPacketGapPicos(pkt._size + sizeTillIpLayer, config);
		if (pkt._direction != nextPkt._direction) {
			minGap = std::max(minGap, config.GetFlowMinDirSwitchGapNanos() * 1000);
		}

		gaps[i] = Gap {i + 1, minGap, minGap};
	}

	return gaps;
}

static std::vector<uint64_t> GenerateRandomGaps(uint64_t numPackets, uint64_t flowDuration)
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
		uint64_t value = (tNorm[i + 1] - tNorm[i]) * flowDuration;
		sum += value;
		gaps[i] = value;
	}

	uint64_t error = flowDuration - sum;
	gaps[rng.RandomUInt(0, gaps.size() - 1)] += error; // Compensate for rounding error

	return gaps;
}

static void Redistribute(
	uint64_t amount,
	uint64_t valueMax,
	std::vector<Gap>& gaps,
	std::size_t& partitionBoundary)
{
	while (amount > 0 && partitionBoundary > 0) {
		std::size_t i = RandomGenerator::GetInstance().RandomUInt(0, partitionBoundary - 1);

		uint64_t amountToAdd = std::min(valueMax - gaps[i].value, amount);
		gaps[i].value += amountToAdd;
		amount -= amountToAdd;

		if (gaps[i].value == valueMax) {
			std::swap(gaps[i], gaps[partitionBoundary - 1]);
			partitionBoundary--;
		}
	}
}

static void ApplyUpperLimit(uint64_t valueMax, std::vector<Gap>& gaps)
{
	auto boundary = std::partition(gaps.begin(), gaps.end(), [=](const Gap& gap) {
		return gap.value < valueMax;
	});
	std::size_t boundaryIndex = std::distance(gaps.begin(), boundary);

	for (std::size_t i = boundaryIndex; i < gaps.size(); i++) {
		Redistribute(gaps[i].value - valueMax, valueMax, gaps, boundaryIndex);
		gaps[i].value = valueMax;
	}
}

static inline uint64_t DivRoundUp(uint64_t a, uint64_t b)
{
	return (a + (b - 1)) / b;
}

static std::vector<Gap> GenerateGaps(
	const std::list<Packet>& packets,
	const ft::Timestamp& tsFirst,
	const ft::Timestamp& tsLast,
	const config::Timestamps& config,
	uint64_t sizeTillIpLayer)
{
	assert(tsFirst >= ft::Timestamp::From<ft::TimeUnit::Seconds>(0));
	assert(tsLast >= ft::Timestamp::From<ft::TimeUnit::Seconds>(0));

	// Prepare and check arguments and handle special cases
	uint64_t start = tsFirst.ToNanoseconds();
	uint64_t end = tsLast.ToNanoseconds();

	if (start > end) {
		throw std::logic_error("start timestamp > end timestamp");
	}

	if (packets.size() == 0) {
		return {};

	} else if (packets.size() == 1) {
		if (start != end) {
			throw std::logic_error("flow has one packet and start timestamp != end timestamp");
		}
		return {Gap {0, 0, 0}};
	}

	// Actual computation of timestamps begins here
	uint64_t wantedDuration = OverflowCheckedMultiply<uint64_t>(end - start, 1000);

	std::vector<Gap> gaps = GenerateMinimalGaps(packets, config, sizeTillIpLayer);
	uint64_t minDuration
		= std::accumulate(gaps.begin(), gaps.end(), uint64_t(0), [](uint64_t acc, const Gap& gap) {
			  return acc + gap.value;
		  });

	uint64_t maxGap = config.GetFlowMaxInterpacketGapNanos();
	if (maxGap
		!= std::numeric_limits<uint64_t>::max()) { // uint64_t::max() is a special default value
		maxGap = OverflowCheckedMultiply<uint64_t>(maxGap, 1000);
	}

	// We aim to for a smaller max interpacket gap than specified so we have extra space to
	// shift packets in the future if we need to without exceeding the specified maximum.
	// Although if it wouldn't be sufficient to reach the desired flow duration, we increase
	// the gap to a value that allows us to reach it, but we still won't pass the maximum specified
	// gap.
	uint64_t targetGap = maxGap / 2;
	uint64_t avgGap = DivRoundUp(wantedDuration, packets.size() - 1);
	if (avgGap > targetGap) {
		targetGap = std::min(avgGap, maxGap);
	}

	if (wantedDuration > minDuration) {
		uint64_t remainingDuration = wantedDuration - minDuration;
		std::vector<uint64_t> randomGaps = GenerateRandomGaps(packets.size(), remainingDuration);
		for (std::size_t i = 0; i < randomGaps.size(); i++) {
			gaps[i].value += randomGaps[i];
		}

		ApplyUpperLimit(targetGap, gaps);
	}

	// Insert an extra "gap" in front of the first packet in case we need to shift it
	gaps.push_back(Gap {0, 0, 0});

	// Each gap is tied to a concrete packet as its calculated with relation to its size
	std::sort(gaps.begin(), gaps.end(), [](const Gap& a, const Gap& b) {
		return a.index < b.index;
	});

	return gaps;
}

TimestampGenerator::TimestampGenerator(
	const std::list<Packet>& packets,
	const ft::Timestamp& tsFirst,
	const ft::Timestamp& tsLast,
	const config::Timestamps& config,
	uint64_t sizeTillIpLayer)
	: _tsFirst(tsFirst)
	, _gaps(GenerateGaps(packets, tsFirst, tsLast, config, sizeTillIpLayer))
{
}

ft::Timestamp TimestampGenerator::Get() const
{
	uint64_t nanos = _tsFirst.ToNanoseconds() + (_accumGap + _gaps[_gapIdx].value) / 1000;
	return ft::Timestamp::From<ft::TimeUnit::Nanoseconds>(nanos);
}

void TimestampGenerator::Next()
{
	_accumGap += _gaps[_gapIdx].value;
	_gapIdx++;
}

void TimestampGenerator::Shift(uint64_t nanosecs)
{
	uint64_t picosecs = nanosecs * 1000;

	_gaps[_gapIdx].value += picosecs;

	for (std::size_t i = _gapIdx + 1; i < _gaps.size(); i++) {
		uint64_t avail = std::min(_gaps[i].value - _gaps[i].valueMin, picosecs);
		_gaps[i].value -= avail;
		picosecs -= avail;
		if (picosecs == 0) {
			break;
		}
	}
}

} // namespace generator
