/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief General utility functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <iomanip>
#include <sstream>
#include <string>
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

} // namespace generator
