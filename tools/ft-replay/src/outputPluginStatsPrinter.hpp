/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief OutputPluginStatsPrinter class for printing statistics of an OutputPlugin.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"
#include "timeConverter.hpp"

#include <chrono>
#include <ctime>
#include <string>
#include <vector>

namespace replay {

/**
 * @brief Prints statistics related to an output plugin.
 */
class OutputPluginStatsPrinter {
public:
	/**
	 * @brief Constructs an OutputPluginStatsPrinter object.
	 * @param outputPlugin Pointer to the output plugin for which statistics will be printed.
	 */
	OutputPluginStatsPrinter(OutputPlugin* outputPlugin);

	/**
	 * @brief Prints the gathered statistics.
	 */
	void PrintStats();

private:
	using msec = std::chrono::milliseconds;

	std::vector<OutputQueueStats> GatherOutputQueueStats(OutputPlugin* outputPlugin);
	void SumarizeOutputQueuesStats(OutputPlugin* outputPlugin);

	void FormatStats();
	std::string FormatTransmissionDuration();
	msec GetTransmissionDuration();
	std::string FormatTransmissionTime(const timespec& transmissionTime);

	OutputQueueStats _outputQueueStats;

	std::string _formattedDuration;
	std::string _formattedStartTime;
	std::string _formattedEndTime;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("OutputPluginStatsPrinter");
};

} // namespace replay
