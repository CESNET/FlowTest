/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Common helper functions used in the the project.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <charconv>
#include <cinttypes>
#include <cstring>
#include <iostream>

/**
 * @brief Parse numeric value from a string.
 * @tparam T type of the value
 * @param str beginning of the string
 * @param value argument where the parsed value should be stored
 * @throws runtime_error parsing error
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
