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

#include <chrono>
#include <string>
#include <vector>

namespace replay {

/**
 * @brief Class for printing statistics of an OutputPlugin.
 *
 * The OutputPluginStatsPrinter class provides functionality to gather and print statistics related
 * to an OutputPlugin. It gathers statistics from the OutputQueueStats of each output queue and
 * summarizes them. The gathered statistics include the number of transmitted packets, transmitted
 * bytes, failed packets, upscaled packets, and transmission time. The class utilizes a logger to
 * output the statistics in a formatted manner.
 */
class OutputPluginStatsPrinter {
public:
	/**
	 * @brief Prints the statistics of an OutputPlugin.
	 *
	 * This function prints the statistics of the given OutputPlugin. It gathers the output queue
	 * statistics using the GatherOutputQueueStats function, summarizes the statistics using
	 * SumarizeOutputQueuesStats, calculates the transmission duration, and formats the output using
	 * the logger.
	 *
	 * @param outputPlugin Pointer to the OutputPlugin for which statistics are to be printed.
	 */
	void PrintStats(OutputPlugin* outputPlugin);

private:
	std::vector<OutputQueueStats> GatherOutputQueueStats(OutputPlugin* outputPlugin);

	OutputQueueStats
	SumarizeOutputQueuesStats(const std::vector<OutputQueueStats>& outputQueuesStats);

	std::chrono::milliseconds TimespecToDuration(timespec ts);
	std::string FormatTime(std::chrono::milliseconds milliseconds);

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("OutputPluginStatsPrinter");
};

} // namespace replay
