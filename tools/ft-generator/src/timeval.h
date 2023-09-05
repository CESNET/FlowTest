/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A wrapper around timeval
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <limits>
#include <stdexcept>
#include <sys/time.h>
#include <time.h>

namespace generator {

/**
 * @brief Class providing a wrapper around timeval with bounds checking,
 *        operators and additional functionality
 */
class Timeval {
public:
	using SecType = decltype(timeval::tv_sec);
	using UsecType = decltype(timeval::tv_usec);

	/**
	 * @brief Construct a new zero timeval object
	 */
	Timeval() = default;

	/**
	 * @brief Construct a new Timeval object
	 *
	 * @param sec The second part
	 * @param usec The microsecond part, between 0 and 1000000
	 *
	 * @throw std::invalid_argument when out of range
	 */
	Timeval(SecType sec, UsecType usec)
		: _value {sec, usec}
	{
		if (usec < 0 || usec >= 1000000) {
			throw std::invalid_argument("timeval usec out of range");
		}
	}

	/**
	 * @brief Construct a new Timeval object
	 *
	 * @param value The underlying timeval
	 *
	 * @throw std::invalid_argument when out of range
	 */
	Timeval(timeval value)
		: _value {value}
	{
		if (value.tv_usec < 0 || value.tv_usec >= 1000000) {
			throw std::invalid_argument("timeval usec out of range");
		}
	}

	/**
	 * @brief Construct a new Timeval object from milliseconds
	 */
	static Timeval FromMilliseconds(int64_t milliseconds);

	/**
	 * @brief Implicit conversion to regular timeval
	 */
	operator timeval() const { return _value; }

	/**
	 * @brief Compare timevals for equality
	 */
	friend bool operator==(const Timeval& a, const Timeval& b)
	{
		return a._value.tv_sec == b._value.tv_sec && a._value.tv_usec == b._value.tv_usec;
	}

	/**
	 * @brief Compare timevals for non-equality
	 */
	friend bool operator!=(const Timeval& a, const Timeval& b) { return !(a == b); }

	/**
	 * @brief Less than comparison of timevals
	 */
	friend bool operator<(const Timeval& a, const Timeval& b)
	{
		if (a._value.tv_sec == b._value.tv_sec) {
			return a._value.tv_usec < b._value.tv_usec;
		} else {
			return a._value.tv_sec < b._value.tv_sec;
		}
	}

	/**
	 * @brief Greater than comparison of timevals
	 */
	friend bool operator>(const Timeval& a, const Timeval& b)
	{
		if (a._value.tv_sec == b._value.tv_sec) {
			return a._value.tv_usec > b._value.tv_usec;
		} else {
			return a._value.tv_sec > b._value.tv_sec;
		}
	}

	/**
	 * @brief Lesser or equal comparison of timevals
	 */
	friend bool operator<=(const Timeval& a, const Timeval& b) { return !(a > b); }

	/**
	 * @brief Greater or equal comparison of timevals
	 */
	friend bool operator>=(const Timeval& a, const Timeval& b) { return !(a < b); }

	/**
	 * @brief Subtract two timevals
	 */
	friend Timeval operator-(Timeval a, Timeval b)
	{
		timeval res;
		timersub(&a._value, &b._value, &res);
		return Timeval(res.tv_sec, res.tv_usec);
	}

	/**
	 * @brief Subtract timeval
	 */
	friend Timeval& operator-=(Timeval& a, const Timeval& b)
	{
		a = a - b;
		return a;
	}

	/**
	 * @brief Convert the timeval to microseconds
	 *
	 * @throw std::overflow_error if the operation would overflow
	 */
	int64_t ToMicroseconds() const;

	/**
	 * @brief Convert the timeval to milliseconds
	 *
	 * @throw std::overflow_error if the operation would overflow
	 */
	int64_t ToMilliseconds() const;

	/**
	 * @brief Get the seconds part of the timeval
	 */
	SecType GetSec() const { return _value.tv_sec; }

	/**
	 * @brief Get the microseconds part of the timeval, always between 0 and 1000000
	 */
	UsecType GetUsec() const { return _value.tv_usec; }

private:
	timeval _value {0, 0};
};

} // namespace generator
