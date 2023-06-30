/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Implementation of the CountDownLatch class for thread synchronization.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "countDownLatch.hpp"

namespace replay {

CountDownLatch::CountDownLatch(size_t eventsCount)
	: _lock()
	, _conditionVariable()
	, _eventsCount(eventsCount)
{
}

void CountDownLatch::Wait()
{
	std::unique_lock<std::mutex> lock(_lock);

	WaitLocked(lock);
}

void CountDownLatch::CountDown(size_t size)
{
	std::unique_lock<std::mutex> lock(_lock);

	CountDownLocked(size);
}

void CountDownLatch::ArriveAndWait(size_t size)
{
	std::unique_lock<std::mutex> lock(_lock);

	CountDownLocked(size);
	WaitLocked(lock);
}

void CountDownLatch::WaitLocked(std::unique_lock<std::mutex>& lock)
{
	if (!_eventsCount) {
		return;
	}

	_conditionVariable.wait(lock, [this]() { return _eventsCount == 0; });
}

void CountDownLatch::CountDownLocked(size_t size)
{
	if (_eventsCount == 0) {
		return;
	}

	_eventsCount -= size;
	if (_eventsCount == 0) {
		_conditionVariable.notify_all();
	}
}

} // namespace replay
