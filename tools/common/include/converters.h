/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary conversion functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace ft {

/**
 * @brief Convert an unsigned integer value to hexadecimal string
 * @tparam T Type of the number
 * @param[in] value Value to convert
 * @return Converted value in the format "0xXXXX" (number of digits depends on the type)
 */
template <typename T, std::enable_if_t<std::is_unsigned_v<T>, bool> = true>
std::string ConvertUintToHex(T value)
{
	const size_t digits = 2 * sizeof(T);
	std::stringstream stream;

	stream << "0x";

	// Since stringstream handles uint8_t as a character, convert the value to uint64_t
	stream << std::setfill('0');
	stream << std::setw(digits);
	stream << std::hex;
	stream << uint64_t(value);

	return stream.str();
}

} // namespace ft
