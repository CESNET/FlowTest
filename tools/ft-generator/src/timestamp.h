/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A timestamp class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace generator {

enum class TimeUnit : int {
	Nanoseconds = 1,
	Microseconds = 2,
	Milliseconds = 3,
	Seconds = 4,
};

/**
 * @brief Class providing a timestamp representation with operators and additional functionality
 */
class Timestamp {
private:
	static constexpr int UnitsInSecond(TimeUnit unit)
	{
		switch (unit) {
		case TimeUnit::Seconds:
			return 1;
		case TimeUnit::Milliseconds:
			return 1'000;
		case TimeUnit::Microseconds:
			return 1'000'000;
		case TimeUnit::Nanoseconds:
			return 1'000'000'000;
		}

		__builtin_unreachable();
	}

	static int64_t SafeAdd(int64_t a, int64_t b)
	{
		assert(b >= 0);
		if (a > std::numeric_limits<int64_t>::max() - b) {
			throw std::overflow_error("operation would overflow");
		}
		return a + b;
	}

	static int64_t SafeMul(int64_t a, int64_t b)
	{
		assert(b >= 0);
		if (b != 0 && a > std::numeric_limits<int64_t>::max() / b) {
			throw std::overflow_error("operation would overflow");
		}
		return a * b;
	}

public:
	/**
	 * @brief Construct a new zero timeval object
	 */
	Timestamp() = default;

	/**
	 * @brief Construct a new Timestamp object
	 *
	 * @param sec The second part
	 * @param nanosec The secondary unit part, between 0 and 1'000'000'000
	 *
	 * @throw std::invalid_argument when out of range
	 */
	Timestamp(int64_t sec, int64_t nanosec)
		: _sec(sec)
		, _nanosec(nanosec)
	{
		if (nanosec < 0 || nanosec >= UnitsInSecond(TimeUnit::Nanoseconds)) {
			throw std::invalid_argument("nanosec out of range");
		}
	}

	/**
	 * @brief Construct a new Timestamp object from a value of the specified time unit
	 */
	template <TimeUnit SourceUnit>
	static Timestamp From(int64_t value)
	{
		int multiplier = UnitsInSecond(TimeUnit::Nanoseconds) / UnitsInSecond(SourceUnit);
		int64_t sec = value / UnitsInSecond(SourceUnit);
		int64_t nanosec = (value % UnitsInSecond(SourceUnit)) * multiplier;
		if (nanosec < 0) {
			sec--;
			nanosec += UnitsInSecond(TimeUnit::Nanoseconds);
		}
		return {sec, nanosec};
	}

	/**
	 * @brief Convert the timeval to nanoseconds
	 *
	 * @throw std::overflow_error if the operation would overflow
	 */
	int64_t ToNanoseconds() const
	{
		return SafeAdd(SafeMul(_sec, UnitsInSecond(TimeUnit::Nanoseconds)), _nanosec);
	}

	/**
	 * @brief Compare timevals for equality
	 */
	friend bool operator==(const Timestamp& a, const Timestamp& b)
	{
		return a._sec == b._sec && a._nanosec == b._nanosec;
	}

	/**
	 * @brief Compare timestamps for non-equality
	 */
	friend bool operator!=(const Timestamp& a, const Timestamp& b) { return !(a == b); }

	/**
	 * @brief Less than comparison of timestamps
	 */
	friend bool operator<(const Timestamp& a, const Timestamp& b)
	{
		if (a._sec == b._sec) {
			return a._nanosec < b._nanosec;
		} else {
			return a._sec < b._sec;
		}
	}

	/**
	 * @brief Greater than comparison of timestamps
	 */
	friend bool operator>(const Timestamp& a, const Timestamp& b)
	{
		if (a._sec == b._sec) {
			return a._nanosec > b._nanosec;
		} else {
			return a._sec > b._sec;
		}
	}

	/**
	 * @brief Lesser or equal comparison of timestamps
	 */
	friend bool operator<=(const Timestamp& a, const Timestamp& b) { return !(a > b); }

	/**
	 * @brief Greater or equal comparison of timestamps
	 */
	friend bool operator>=(const Timestamp& a, const Timestamp& b) { return !(a < b); }

	/**
	 * @brief Subtract two timestamps
	 */
	friend Timestamp operator-(Timestamp a, Timestamp b)
	{
		int64_t sec = a._sec - b._sec;
		int64_t nanosec = a._nanosec - b._nanosec;
		if (nanosec < 0) {
			sec--;
			nanosec += UnitsInSecond(TimeUnit::Nanoseconds);
		}
		return {sec, nanosec};
	}

	/**
	 * @brief Subtract timestamp
	 */
	friend Timestamp& operator-=(Timestamp& a, const Timestamp& b)
	{
		a = a - b;
		return a;
	}

	/**
	 * @brief Convert timestamp to string
	 */
	template <TimeUnit PrintUnit>
	std::string ToString() const
	{
		std::stringstream ss;

		if (PrintUnit > TimeUnit::Nanoseconds) {
			int divisor = UnitsInSecond(TimeUnit::Nanoseconds) / UnitsInSecond(PrintUnit);

			int64_t value = ToNanoseconds();
			int64_t whole = value / divisor;
			int64_t decimal = std::abs(value % divisor); // Modulo of a negative value is negative
			int decimalPlaces = (int(PrintUnit) - int(TimeUnit::Nanoseconds)) * 3;

			ss << whole << "." << std::setfill('0') << std::setw(decimalPlaces) << decimal;

		} else {
			ss << ToNanoseconds();
		}

		return ss.str();
	}

	/**
	 * @brief Get the second part of the value
	 */
	int64_t SecPart() const { return _sec; }

	/**
	 * @brief Get the nanosecond part of the value, returned value will be in range
	 *        <0, 1'000'000'000)
	 */
	int64_t NanosecPart() const { return _nanosec; }

private:
	int64_t _sec = 0;
	int64_t _nanosec = 0;
};

} // namespace generator
