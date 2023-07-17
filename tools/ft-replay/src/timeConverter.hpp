/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the TimeConverter class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cassert>
#include <chrono>
#include <ctime>

namespace replay {

/**
 * @brief Utility class for time conversion operations.
 */
class TimeConverter {
public:
	/**
	 * Convert a timespec value to a chrono duration of type T.
	 * @tparam T The desired duration type.
	 * @param timespecToConvert The timespec value to convert.
	 * @return The converted duration of type T.
	 */
	template <typename T>
	T TimespecToDuration(const timespec& timespecToConvert) const
	{
		auto timespecInChrono = std::chrono::seconds {timespecToConvert.tv_sec}
			+ std::chrono::nanoseconds {timespecToConvert.tv_nsec};
		return std::chrono::duration_cast<T>(timespecInChrono);
	}

	/**
	 * Convert a system time in timespec format to a chrono duration epoch time of type T.
	 * @tparam T The desired epoch time type.
	 * @param systemTime The system time in timespec format to convert.
	 * @return The converted chrono duration epoch time of type T.
	 */
	template <typename T>
	T SystemTimeToEpochTime(const timespec& systemTime) const
	{
		auto epochTimeNow = GetEpochTimeNow<T>();
		auto systemTimeNow = GetSystemTimeNow<T>();
		auto systemTimeToConvert = TimespecToDuration<T>(systemTime);

		assert(systemTimeNow.count() >= systemTimeToConvert.count());
		auto systemTimeDifference = systemTimeNow - systemTimeToConvert;
		return epochTimeNow - systemTimeDifference;
	}

private:
	template <typename T>
	T GetEpochTimeNow() const
	{
		auto timeNowSinceEpoch = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration_cast<T>(timeNowSinceEpoch);
	}

	template <typename T>
	T GetSystemTimeNow() const
	{
		timespec systemTimeNow;
		clock_gettime(CLOCK_MONOTONIC_COARSE, &systemTimeNow);
		return TimespecToDuration<T>(systemTimeNow);
	}
};

} // namespace replay
