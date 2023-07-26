/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size probability configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetsizeprobabilities.h"
#include "common.h"

#include <pcapplusplus/EthLayer.h>

#include <algorithm>
#include <numeric>

namespace generator {
namespace config {

const static std::vector<IntervalInfo> DEFAULT_PACKET_SIZE_PROBABILITIES
	= {{64, 79, 0.2824},
	   {80, 159, 0.073},
	   {160, 319, 0.0115},
	   {320, 639, 0.012},
	   {640, 1279, 0.0092},
	   {1280, 1500, 0.6119}};

static constexpr int ETHER_HDR_SIZE = sizeof(pcpp::ether_header);

static std::vector<IntervalInfo> AdjustPacketSizesToL3(std::vector<IntervalInfo> intervals)
{
	for (IntervalInfo& interval : intervals) {
		if (interval._from < ETHER_HDR_SIZE || interval._to < ETHER_HDR_SIZE) {
			throw std::invalid_argument("value must be at least the size of an L2 header");
		}
		interval._from -= ETHER_HDR_SIZE;
		interval._to -= ETHER_HDR_SIZE;
	}
	return intervals;
}

static bool IntervalContains(const IntervalInfo& interval, uint64_t value)
{
	return interval._from <= value && value <= interval._to;
}

static bool IntervalsOverlap(const IntervalInfo& a, const IntervalInfo& b)
{
	return IntervalContains(a, b._from) || IntervalContains(a, b._to)
		|| IntervalContains(b, a._from) || IntervalContains(b, a._to);
}

static std::vector<IntervalInfo> NormalizeIntervals(std::vector<IntervalInfo> intervals)
{
	if (intervals.empty()) {
		return intervals;
	}

	// Sort the intervals based on their ranges
	std::sort(intervals.begin(), intervals.end(), [](const IntervalInfo& a, const IntervalInfo& b) {
		return a._from < b._from;
	});

	// Make sure the probabilities sum to one
	double totalProbability = std::accumulate(
		intervals.begin(),
		intervals.end(),
		0.0,
		[](double acc, const IntervalInfo& interval) { return acc + interval._probability; });
	for (auto& interval : intervals) {
		interval._probability /= totalProbability;
	}

	// Fill in gaps
	std::vector<IntervalInfo> tmp;
	auto prevTo = intervals[0]._from;
	bool first = true;
	for (const auto& interval : intervals) {
		if (prevTo + 1 != interval._from && !first) {
			tmp.push_back({prevTo + 1, interval._from - 1, 0.0});
		}
		tmp.push_back(interval);
		prevTo = interval._to;
		first = false;
	}
	intervals = std::move(tmp);

	// Adjust to L3
	intervals = AdjustPacketSizesToL3(intervals);

	return intervals;
}

PacketSizeProbabilities::PacketSizeProbabilities()
	: _intervals(DEFAULT_PACKET_SIZE_PROBABILITIES)
	, _normalizedIntervals(NormalizeIntervals(_intervals))
{
}

PacketSizeProbabilities::PacketSizeProbabilities(const YAML::Node& node)
{
	ExpectMap(node);

	double totalProbability = 0.0;
	for (const auto& p : node) {
		const auto rangeErr
			= ConfigError(p.first, "expected range in format \"<FROM>-<TO>\", i.e. 100-200");

		const auto& range = AsScalar(p.first);
		const auto& pieces = StringSplit(range, "-");
		if (pieces.size() != 2) {
			throw rangeErr;
		}
		auto from = ParseValue<uint16_t>(pieces[0]);
		auto to = ParseValue<uint16_t>(pieces[1]);
		if (!from || !to || *from > *to) {
			throw rangeErr;
		}

		double probability = ParseProbability(p.second);

		IntervalInfo interval {*from, *to, probability};
		for (const auto& other : _intervals) {
			if (!IntervalsOverlap(interval, other)) {
				continue;
			}

			const auto range = std::to_string(other._from) + "-" + std::to_string(other._to);
			throw ConfigError(
				p.first,
				"interval range overlaps with previously defined range " + range);
		}

		_intervals.push_back({*from, *to, probability});
		totalProbability += probability;
	}

	if (_intervals.empty()) {
		throw ConfigError(
			node,
			"at least one packet size probability must be defined (if you want default values to "
			"be used, omit the configuration key entirely)");
	}

	if (totalProbability == 0.0) {
		throw ConfigError(node, "total probability cannot be 0");
	}

	_normalizedIntervals = NormalizeIntervals(_intervals);
}

} // namespace config
} // namespace generator
