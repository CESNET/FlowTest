/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Implementation of the OutputPluginStatsPrinter class for printing statistics of an
 * OutputPlugin.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "outputPluginStatsPrinter.hpp"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace replay {

void OutputPluginStatsPrinter::PrintStats(OutputPlugin* outputPlugin)
{
	auto outputQueuesStats = GatherOutputQueueStats(outputPlugin);
	OutputQueueStats statsSum = SumarizeOutputQueuesStats(outputQueuesStats);

	auto transmissionDuration = TimespecToDuration(statsSum.transmitEndTime)
		- TimespecToDuration(statsSum.transmitStartTime);

	_logger->info(
		"Actual: {} packets ({} bytes) sent in {} seconds",
		statsSum.transmittedPackets,
		statsSum.transmittedBytes,
		FormatTime(transmissionDuration));
	_logger->info("Output plugin statistics:");
	_logger->info("    Successful packets:  {}", statsSum.transmittedPackets);
	_logger->info("    Failed packets:      {}", statsSum.failedPackets);
	_logger->info("    Upscaled packets:    {}", statsSum.upscaledPackets);
}

std::vector<OutputQueueStats>
OutputPluginStatsPrinter::GatherOutputQueueStats(OutputPlugin* outputPlugin)
{
	std::vector<OutputQueueStats> outputQueuesStats;
	for (size_t queueId = 0; queueId < outputPlugin->GetQueueCount(); queueId++) {
		const OutputQueue* outputQueue = outputPlugin->GetQueue(queueId);
		outputQueuesStats.emplace_back(outputQueue->GetOutputQueueStats());
	}
	return outputQueuesStats;
}

OutputQueueStats OutputPluginStatsPrinter::SumarizeOutputQueuesStats(
	const std::vector<OutputQueueStats>& outputQueuesStats)
{
	OutputQueueStats statsSum;
	auto outputQueuesStatsSum = [](OutputQueueStats sum, const OutputQueueStats& queueStats) {
		sum.transmittedPackets += queueStats.transmittedPackets;
		sum.transmittedBytes += queueStats.transmittedBytes;
		sum.failedPackets += queueStats.failedPackets;
		sum.upscaledPackets += queueStats.upscaledPackets;
		return sum;
	};

	statsSum = std::accumulate(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		OutputQueueStats(),
		outputQueuesStatsSum);

	auto outputQueuesStatsTimeMin = [this](const OutputQueueStats& a, const OutputQueueStats& b) {
		return TimespecToDuration(a.transmitStartTime) < TimespecToDuration(b.transmitStartTime);
	};

	auto transmitStartTimeIterator = std::min_element(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		outputQueuesStatsTimeMin);
	statsSum.transmitStartTime = transmitStartTimeIterator->transmitStartTime;

	auto outputQueuesStatsTimeMax = [this](const OutputQueueStats& a, const OutputQueueStats& b) {
		return TimespecToDuration(a.transmitEndTime) > TimespecToDuration(b.transmitEndTime);
	};

	auto transmitEndTimeIterator = std::max_element(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		outputQueuesStatsTimeMax);
	statsSum.transmitEndTime = transmitEndTimeIterator->transmitEndTime;

	return statsSum;
}

std::chrono::milliseconds OutputPluginStatsPrinter::TimespecToDuration(timespec ts)
{
	auto duration = std::chrono::seconds {ts.tv_sec} + std::chrono::nanoseconds {ts.tv_nsec};
	return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
}

std::string OutputPluginStatsPrinter::FormatTime(std::chrono::milliseconds milliseconds)
{
	using namespace std::chrono_literals;
	static constexpr uint64_t MillisecInSec = std::chrono::milliseconds(1s).count();
	std::stringstream result;

	const auto partSec = milliseconds.count() / MillisecInSec;
	const auto partMsec = milliseconds.count() % MillisecInSec;

	result << partSec;
	result << ".";
	result << std::setfill('0') << std::setw(3) << std::to_string(partMsec);

	return result.str();
}

} // namespace replay
