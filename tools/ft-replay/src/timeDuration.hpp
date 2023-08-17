/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the TimeDuration class for calculating duration based on timestamps.
 *
 * This file contains the definition of the TimeDuration class, which is used to calculate
 * and manage duration based on timestamps. The class allows updating timestamps and
 * obtaining the duration.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdint.h>

namespace replay {

/**
 * @brief Class for calculating duration based on timestamps.
 *
 * This class enables the calculation of duration between the oldest and newest timestamps.
 * Timestamps can be updated, and the duration can be obtained.
 */
class TimeDuration {
public:
	/**
	 * @brief Constructs an instance of the TimeDuration class.
	 */
	TimeDuration();

	/**
	 * @brief Updates timestamps based on the provided timestamp.
	 *
	 * @param currentTimestamp The timestamp to update.
	 */
	void Update(uint64_t currentTimestamp) noexcept;

	/**
	 * @brief Gets the duration between the oldest and newest timestamps.
	 *
	 * @return Duration between timestamps. Zero if never updated.
	 */
	uint64_t GetDuration() const noexcept;

private:
	bool _isInitailized;
	uint64_t _oldestTimestamp;
	uint64_t _newestTimestamp;
};

} // namespace replay
