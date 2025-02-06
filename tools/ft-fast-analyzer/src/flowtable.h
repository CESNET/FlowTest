/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Defines the FlowTable class for managing and processing network flow records.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "flow.h"
#include "logger.h"

#include <string_view>
#include <vector>

namespace ft::analyzer {

class FlowTable {
public:
	static constexpr auto CSV_FORMAT
		= "START_TIME,END_TIME,PROTOCOL,SRC_IP,DST_IP,SRC_PORT,DST_PORT,PACKETS,BYTES";

	/**
	 * @brief Constructs a FlowTable and initializes it by parsing a CSV file.
	 *        Optionally adjusts flow timestamps based on a provided start time.
	 * @param path Path to the input CSV file.
	 * @param startTime Optional start time to adjust flow timestamps.
	 * @throws std::runtime_error if the file cannot be opened, mapped, or has an invalid header.
	 */
	FlowTable(std::string_view path, uint64_t startTime = 0);

	/**
	 * @brief Retrieves the list of flows stored in the table.
	 * @return A constant reference to the vector of flows.
	 */
	const std::vector<Flow>& GetFlows() const;

private:
	/**
	 * @brief Parses the input file and creates a flow record from each line.
	 *        Assumes the file is already mapped into memory.
	 * @param start Pointer to the beginning of the file content.
	 * @param end Pointer to the end of the file content.
	 */
	void ParseProfileFile(const char* start, const char* end);

	/* data */
	std::vector<Flow> _flows;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("FlowTable");
};

} // namespace ft::analyzer
