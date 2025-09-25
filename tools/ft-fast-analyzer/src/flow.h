/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Defines the Flow structure and related utilities for processing flow records in CSV
 * format.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "ipaddr.h"
#include <string_view>

namespace ft::analyzer {

/**
 * @brief Structure representing a flow record present in the CSV file.
 */
struct Flow {
	/**
	 * @brief Initializes a Flow object from a single flow record in the CSV file.
	 *
	 * Expected format:
	 * START_TIME,END_TIME,PROTOCOL,SRC_IP,DST_IP,SRC_PORT,DST_PORT,PACKETS,BYTES
	 *
	 * @param record A flow record in the expected format.
	 * @throws std::runtime_error On parsing errors of individual record fields.
	 */
	explicit Flow(std::string_view record);

	/**
	 * @brief Parses a flow field out of a CSV line.
	 * @tparam T Type of the value.
	 * @param line Beginning of the line.
	 * @param value Argument where the parsed value should be stored.
	 * @return Rest of the line after parsing the field.
	 * @throws runtime_error Parsing error.
	 */
	template <typename T>
	std::string_view ConsumeField(std::string_view line, T& value);

	uint64_t start_time {};
	uint64_t end_time {};
	IPAddr src_ip {};
	IPAddr dst_ip {};
	uint16_t src_port {};
	uint16_t dst_port {};
	uint16_t l4_proto {};
	uint64_t packets {};
	uint64_t bytes {};
};

} // namespace ft::analyzer
