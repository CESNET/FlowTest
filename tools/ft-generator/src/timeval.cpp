/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A wrapper around timeval
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "timeval.h"

namespace generator {

Timeval Timeval::FromMilliseconds(int64_t milliseconds)
{
	timeval time;
	time.tv_sec = milliseconds / 1000;
	time.tv_usec = (milliseconds % 1000) * 1000;
	if (time.tv_usec < 0) {
		time.tv_sec--;
		time.tv_usec += 1000000;
	}
	return Timeval {time};
}

int64_t Timeval::ToMicroseconds() const
{
	int64_t microseconds = 0;

	if (_value.tv_sec > std::numeric_limits<int64_t>::max() / 1000000
		|| _value.tv_sec < std::numeric_limits<int64_t>::min() / 1000000) {
		throw std::overflow_error("cannot convert timeval to microseconds due to overflow");
	}
	microseconds += _value.tv_sec * 1000000;

	if (std::numeric_limits<int64_t>::max() - microseconds < _value.tv_usec) {
		throw std::overflow_error("cannot convert timeval to microseconds due to overflow");
	}
	microseconds += _value.tv_usec;

	return microseconds;
}

int64_t Timeval::ToMilliseconds() const
{
	int64_t milliseconds = 0;

	if (_value.tv_sec > std::numeric_limits<int64_t>::max() / 1000
		|| _value.tv_sec < std::numeric_limits<int64_t>::min() / 1000) {
		throw std::overflow_error("cannot convert timeval to milliseconds due to overflow");
	}
	milliseconds += _value.tv_sec * 1000;

	if (std::numeric_limits<int64_t>::max() - milliseconds < _value.tv_usec / 1000) {
		throw std::overflow_error("cannot convert timeval to milliseconds due to overflow");
	}
	milliseconds += _value.tv_usec / 1000;

	return milliseconds;
}

} // namespace generator
