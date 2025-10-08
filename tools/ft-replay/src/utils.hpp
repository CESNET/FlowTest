/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary utilities
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "threadManager.hpp"
#include <charconv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <string_view>

namespace replay::utils {

/**
 * @brief Parse numeric value from a string.
 * @tparam T type of the value
 * @param str beginning of the string
 * @param key name of the argument used in error messages
 * @param value argument where the parsed value should be stored
 * @throws runtime_error parsing error
 */
template <typename T>
void FromString(std::string_view str, std::string_view key, T& value)
{
	const char* begin = str.data();
	const char* end = begin + str.size();
	std::string err {key};

	auto [ptr, ec] {std::from_chars(begin, end, value)};
	if (ec == std::errc() && ptr == end) {
		return;
	}

	const std::string valueStr {str};

	if (ec == std::errc::result_out_of_range) {
		throw std::runtime_error("'" + valueStr + "' is out of range in argument " + err);
	} else if (ec == std::errc::invalid_argument) {
		throw std::runtime_error("'" + valueStr + "' is not a valid number in argument " + err);
	} else {
		throw std::runtime_error(
			"'" + valueStr + "' is not a valid number due to unexpected characters in argument "
			+ err);
	}
}

/**
 * @brief Parse numeric values from a string delimited by delimiter
 * @tparam T type of the values
 * @param str string to parse
 * @param delimiter separates the numeric values inside the string
 * @param key name of the argument used in error messages
 * @throws invalid_argument argument error
 * @return set of the parsed values
 */
template <typename T>
std::set<T>
ParseListOfNumbers(std::string_view str, std::string_view delimiter, std::string_view key)
{
	std::set<T> resultSet;

	size_t start = 0;
	size_t end = 0;

	while (end != std::string::npos) {
		end = str.find(delimiter, start);
		std::string resultString(str.substr(start, end - start));

		if (resultString.empty()) {
			throw std::invalid_argument("Empty value given in argument: " + std::string(key));
		}

		size_t rangeDelimiter = resultString.find('-');
		if (rangeDelimiter != std::string::npos) {
			std::string startStr = resultString.substr(0, rangeDelimiter);
			std::string endStr = resultString.substr(rangeDelimiter + 1);

			if (startStr.empty() || endStr.empty()) {
				throw std::invalid_argument("Malformed range in argument: " + std::string(key));
			}

			T rangeStart, rangeEnd;
			utils::FromString<T>(startStr, key, rangeStart);
			utils::FromString<T>(endStr, key, rangeEnd);

			if (rangeEnd < rangeStart) {
				throw std::invalid_argument(
					"Invalid range: end < start in argument: " + std::string(key));
			}

			for (T val = rangeStart; val <= rangeEnd; ++val) {
				resultSet.insert(val);
			}
		} else {
			T value;
			utils::FromString<T>(resultString, key, value);
			resultSet.insert(value);
		}

		start = end + delimiter.length();
	}

	return resultSet;
}

/**
 * @brief Perform case insensitive comparison of two strings.
 * @return True if equal. False otherwise.
 */
bool CaseInsensitiveCompare(std::string_view lhs, std::string_view rhs);

/**
 * @brief Test if the given value is power of two.
 */
bool PowerOfTwo(uint64_t value);

/**
 * @brief Convert the given string to boolean value.
 *
 * Following boolean values are supported: yes/no, true/false, on/off, 0/1.
 * The comparison is case insensitive.
 * @param[in] str String to convert.
 * @throw std::invalid_argument If the string cannot be converted.
 */
bool StrToBool(std::string_view str);

/**
 * @brief Get MTU of a network interface.
 *
 * Keep on mind that the returned value doesn't contain Ethenet header.
 * @param[in] name Network interface name.
 * @throw std::runtime_error if the MTU cannot be obtained.
 */
uint16_t GetInterfaceMTU(const std::string& name);

/**
 * @brief Get NUMA node of the specified network interface
 *
 */
NumaNode GetInterfaceNumaNode(const std::string& interface);

} // namespace replay::utils
