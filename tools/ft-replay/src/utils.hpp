/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary utilities
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace replay::utils {

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

} // namespace replay::utils
