/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Defines the Flow structure and related utilities for processing flow records in CSV
 * format.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flow.h"
#include <charconv>
#include <ostream>
#include <stdexcept>

namespace ft::analyzer {

/**
 * @brief Parses a numeric value.
 * @tparam T Type of the value.
 * @param str Beginning of the string.
 * @param value Argument where the parsed value should be stored.
 * @throws runtime_error Parsing error.
 */
template <typename T>
void FromString(std::string_view str, T& value)
{
	const char* begin = str.data();
	const char* end = begin + str.size();

	auto [ptr, ec] {std::from_chars(begin, end, value)};
	if (ec == std::errc() && ptr == end) {
		return;
	}

	const std::string valueStr {str};

	if (ec == std::errc::result_out_of_range) {
		throw std::runtime_error("'" + valueStr + "' is out of range");
	} else if (ec == std::errc::invalid_argument) {
		throw std::runtime_error("'" + valueStr + "' is not a valid number");
	} else {
		throw std::runtime_error(
			"'" + valueStr + "' is not a valid number due to unexpected characters");
	}
}

/**
 * @brief Parses an IP address from a string (specialization for IP address).
 * @param str Beginning of the string.
 * @param value Argument where the parsed value should be stored.
 * @throws invalid_argument Parsing error.
 */
template <>
void FromString(std::string_view str, IPAddr& value)
{
	const std::string valueStr {str};
	value = IPAddr(valueStr);
}

Flow::Flow(std::string_view record)
{
	record = ConsumeField(record, start_time);
	record = ConsumeField(record, end_time);
	record = ConsumeField(record, l4_proto);

	record = ConsumeField(record, src_ip);
	record = ConsumeField(record, dst_ip);

	record = ConsumeField(record, src_port);
	record = ConsumeField(record, dst_port);
	record = ConsumeField(record, packets);
	record = ConsumeField(record, bytes);

	if (!record.empty()) {
		throw std::runtime_error("Unexpected field: '" + std::string(record) + "'");
	}

	if (packets == 0) {
		throw std::runtime_error("Sum of packets in a biflow record cannot be zero");
	}

	if (bytes == 0) {
		throw std::runtime_error("Sum of bytes in a biflow record cannot be zero");
	}
}

template <typename T>
std::string_view Flow::ConsumeField(std::string_view line, T& value)
{
	const auto nextFieldPos = line.find(',');
	std::string_view field = line.substr(0, nextFieldPos);

	FromString(field, value);
	if (nextFieldPos == std::string_view::npos) {
		return {};
	} else {
		return line.substr(nextFieldPos + 1);
	}
}

} // namespace ft::analyzer
