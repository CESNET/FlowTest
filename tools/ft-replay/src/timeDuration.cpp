/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Implementation of the TimeDuration class for calculating duration based on timestamps.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "timeDuration.hpp"

#include <limits>

namespace replay {

TimeDuration::TimeDuration()
	: _isInitailized(false)
	, _oldestTimestamp(std::numeric_limits<uint64_t>::max())
	, _newestTimestamp(0)
{
}

void TimeDuration::Update(uint64_t currentTimestamp) noexcept
{
	_isInitailized = true;

	if (_oldestTimestamp > currentTimestamp) {
		_oldestTimestamp = currentTimestamp;
	}

	if (_newestTimestamp < currentTimestamp) {
		_newestTimestamp = currentTimestamp;
	}
}

uint64_t TimeDuration::GetDuration() const noexcept
{
	if (!_isInitailized) {
		return 0;
	}

	return _newestTimestamp - _oldestTimestamp;
}

} // namespace replay
