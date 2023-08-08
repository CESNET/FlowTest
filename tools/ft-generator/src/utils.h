/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief General utility functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace generator {

/**
 * @brief Calculate the absolute difference of two values
 */
template <typename T>
T AbsDiff(T a, T b)
{
	return a > b ? a - b : b - a;
}

/**
 * @brief Safe division of values, where dividing by 0 results in 0
 */
template <typename T>
T SafeDiv(T a, T b)
{
	return b == 0 ? 0 : a / b;
}

/**
 * @brief Safe subtraction of unsigned values, where what would be an underflow results in a 0
 */
template <typename T>
// Enable only when T is an unsigned integer type
typename std::enable_if<std::is_unsigned_v<T>, T>::type SafeSub(T a, T b)
{
	return a > b ? a - b : 0;
}

/**
 * @brief Calculate a "difference ratio", i.e. what's the ratio of the difference of two values
 *        compared to the desired value, or in other words, a normalized difference of two values.
 */
template <typename T>
double DiffRatio(T desired, T actual)
{
	return SafeDiv<double>(AbsDiff(desired, actual), desired);
}

/**
 * @brief Format a value as a string with metric units suffix (k, M, G, T, P)
 */
template <typename T>
static std::string ToMetricUnits(T value)
{
	static const std::vector<std::string> Suffixes {"", "k", "M", "G", "T", "P"};

	double adjValue = value;
	size_t i = 0;
	while (i < Suffixes.size() - 1 && adjValue > 1000.0) {
		adjValue /= 1000.0;
		i++;
	}

	std::stringstream ss;
	ss << std::fixed << std::setprecision(2) << adjValue << " " << Suffixes[i];
	return ss.str();
}

/**
 * @brief Multiplication of unsigned values that checks for overflow
 *
 * @throw std::overflow_error if the operation would overflow
 */
template <typename T>
// Enable only when T is an unsigned integer type
typename std::enable_if<std::is_unsigned_v<T>, T>::type OverflowCheckedMultiply(T a, T b)
{
	if ((b > 0 && a > std::numeric_limits<T>::max() / b)
		|| (a > 0 && b > std::numeric_limits<T>::max() / a)) {
		throw std::overflow_error("multiplication of values would overflow");
	}
	return a * b;
}

} // namespace generator
