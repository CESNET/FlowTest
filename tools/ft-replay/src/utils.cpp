/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary utilities
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace replay::utils {

bool CaseInsensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	const auto cmp = [](char lhsLetter, char rhsLetter) {
		return std::tolower(lhsLetter) == std::tolower(rhsLetter);
	};

	return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), cmp);
}

bool PowerOfTwo(uint64_t value)
{
	return value && (!(value & (value - 1)));
}

bool StrToBool(std::string_view str)
{
	for (const auto& item : {"yes", "true", "on", "1"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return true;
		}
	}

	for (const auto& item : {"no", "false", "off", "0"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return false;
		}
	}

	throw std::invalid_argument("Unable to convert '" + std::string(str) + "' to boolean");
}

} // namespace replay::utils
