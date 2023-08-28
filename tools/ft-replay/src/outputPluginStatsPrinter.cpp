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
#include <cassert>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace replay {

OutputPluginStatsPrinter::OutputPluginStatsPrinter(OutputPlugin* outputPlugin)
{
	SumarizeOutputQueuesStats(outputPlugin);
	FormatStats();
}

void OutputPluginStatsPrinter::SumarizeOutputQueuesStats(OutputPlugin* outputPlugin)
{
	auto outputQueuesStats = GatherOutputQueueStats(outputPlugin);

	auto outputQueuesStatsSum = [](OutputQueueStats sum, const OutputQueueStats& queueStats) {
		sum.transmittedPackets += queueStats.transmittedPackets;
		sum.transmittedBytes += queueStats.transmittedBytes;
		sum.failedPackets += queueStats.failedPackets;
		sum.upscaledPackets += queueStats.upscaledPackets;
		return sum;
	};

	_outputQueueStats = std::accumulate(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		OutputQueueStats(),
		outputQueuesStatsSum);

	auto outputQueuesStatsTimeMin = [](const OutputQueueStats& a, const OutputQueueStats& b) {
		TimeConverter timeConverter;
		return timeConverter.TimespecToDuration<msec>(a.transmitStartTime)
			< timeConverter.TimespecToDuration<msec>(b.transmitStartTime);
	};

	std::vector<OutputQueueStats> activeQueues;
	std::copy_if(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		std::back_inserter(activeQueues),
		[](const OutputQueueStats& stats) { return stats.transmittedBytes; });

	auto transmitStartTimeIterator
		= std::min_element(activeQueues.begin(), activeQueues.end(), outputQueuesStatsTimeMin);
	if (transmitStartTimeIterator != activeQueues.end()) {
		_outputQueueStats.transmitStartTime = transmitStartTimeIterator->transmitStartTime;
	}

	auto outputQueuesStatsTimeMax = [](const OutputQueueStats& a, const OutputQueueStats& b) {
		TimeConverter timeConverter;
		return timeConverter.TimespecToDuration<msec>(a.transmitEndTime)
			< timeConverter.TimespecToDuration<msec>(b.transmitEndTime);
	};

	auto transmitEndTimeIterator = std::max_element(
		outputQueuesStats.begin(),
		outputQueuesStats.end(),
		outputQueuesStatsTimeMax);
	_outputQueueStats.transmitEndTime = transmitEndTimeIterator->transmitEndTime;
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

void OutputPluginStatsPrinter::FormatStats()
{
	_formattedDuration = FormatTransmissionDuration();
	_formattedStartTime = FormatTransmissionTime(_outputQueueStats.transmitStartTime);
	_formattedEndTime = FormatTransmissionTime(_outputQueueStats.transmitEndTime);
}

std::string OutputPluginStatsPrinter::FormatTransmissionDuration()
{
	auto durationTime = GetTransmissionDuration();

	using namespace std::chrono_literals;
	static constexpr uint64_t MillisecInSec = std::chrono::milliseconds(1s).count();
	std::stringstream result;

	const auto partSec = durationTime.count() / MillisecInSec;
	const auto partMsec = durationTime.count() % MillisecInSec;

	result << partSec;
	result << ".";
	result << std::setfill('0') << std::setw(3) << std::to_string(partMsec);

	return result.str();
}

OutputPluginStatsPrinter::msec OutputPluginStatsPrinter::GetTransmissionDuration()
{
	TimeConverter timeConverter;

	auto startTime = timeConverter.TimespecToDuration<msec>(_outputQueueStats.transmitStartTime);
	auto endTime = timeConverter.TimespecToDuration<msec>(_outputQueueStats.transmitEndTime);

	return endTime - startTime;
}

std::string OutputPluginStatsPrinter::FormatTransmissionTime(const timespec& transmissionTime)
{
	TimeConverter timeConverter;

	auto transmissionEpochTime = timeConverter.SystemTimeToEpochTime<msec>(transmissionTime);
	time_t transmissionEpochSeconds
		= std::chrono::duration_cast<std::chrono::seconds>(transmissionEpochTime).count();

	std::ostringstream formattedTime;
	formattedTime << std::put_time(std::localtime(&transmissionEpochSeconds), "%Y-%m-%d %H:%M:%S");
	formattedTime << " [ms since epoch: " + std::to_string(transmissionEpochTime.count()) + "]";

	return formattedTime.str();
}

void OutputPluginStatsPrinter::PrintStats()
{
	_logger->info(
		"Actual: {} packets ({} bytes) sent in {} seconds",
		_outputQueueStats.transmittedPackets,
		_outputQueueStats.transmittedBytes,
		_formattedDuration);
	_logger->info("Start time:\t" + _formattedStartTime);
	_logger->info("End time:\t" + _formattedEndTime);
	_logger->info("Output plugin statistics:");
	_logger->info("    Successful packets:  {}", _outputQueueStats.transmittedPackets);
	_logger->info("    Failed packets:      {}", _outputQueueStats.failedPackets);
	_logger->info("    Upscaled packets:    {}", _outputQueueStats.upscaledPackets);
}

} // namespace replay
