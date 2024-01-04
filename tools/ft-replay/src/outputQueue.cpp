/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "outputQueue.hpp"

namespace replay {

void OutputQueueStats::UpdateTime() noexcept
{
	timespec now;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
	transmitEndTime = now;

	if (!transmitStartTime.tv_sec && !transmitStartTime.tv_nsec) {
		transmitStartTime = now;
	}
}

void OutputQueue::Flush() {}

} // namespace replay
