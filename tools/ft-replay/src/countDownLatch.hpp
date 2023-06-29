/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Defines the CountDownLatch class for thread synchronization.
 *
 * This file provides the implementation of the CountDownLatch class, which allows one or more
 * threads to wait until a specified number of events occur before proceeding.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <condition_variable>
#include <mutex>

namespace replay {

/**
 * @brief A synchronization primitive that allows threads to wait until a specified number of events
 * occur.
 *
 * The CountDownLatch class provides a mechanism for threads to wait until a specified number of
 * events have occurred before proceeding. Each event decrements the internal count, and threads
 * calling the Wait() function will block until the count reaches zero. The CountDown() function is
 * used to decrement the count when an event occurs.
 */
class CountDownLatch {
public:
	/**
	 * @brief Constructs a CountDownLatch object with the specified number of events.
	 *
	 * @param eventsCount The number of events that need to occur before threads can proceed.
	 */
	CountDownLatch(size_t eventsCount);

	CountDownLatch(const CountDownLatch& countDownLatch) = delete;
	CountDownLatch(CountDownLatch&& countDownLatch) = delete;
	CountDownLatch& operator=(const CountDownLatch& countDownLatch) = delete;
	CountDownLatch& operator=(CountDownLatch&& countDownLatch) = delete;

	/**
	 * @brief Waits until the count reaches zero.
	 *
	 * Threads calling this function will block until the count reaches zero, indicating that all
	 * events have occurred. If the count is already zero, the function returns immediately.
	 */
	void Wait();

	/**
	 * @brief Decrements the count by the specified size.
	 *
	 * This function is called to indicate that an event has occurred. It decrements the internal
	 * count and, if the count reaches zero, unblocks all waiting threads.
	 *
	 * @note If @p size is greater than the value of the internal counter or is negative, the
	 * behavior is undefined.
	 */
	void CountDown(size_t size = 1);

	/**
	 * @brief Decrements the count by the specified size and waits until the count reaches zero.
	 *
	 * This function is a convenience method that decrements the count of the associated
	 * CountDownLatch object by the specified size and then waits until the count reaches zero. It
	 * is faster than calling `CountDown(size)` followed by `Wait()`.
	 *
	 * @param size The number by which to decrement the count. Defaults to 1.
	 *
	 * @note If @p size is greater than the value of the internal counter or is negative, the
	 * behavior is undefined.
	 */
	void ArriveAndWait(size_t size = 1);

private:
	void WaitLocked(std::unique_lock<std::mutex>& lock);
	void CountDownLocked(size_t size);

	std::mutex _lock;
	std::condition_variable _conditionVariable;
	size_t _eventsCount;
};

} // namespace replay
