/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config.hpp"
#include "logger.h"
#include "rateLimiter.hpp"

#include <cstddef>

namespace replay {

/**
 * @brief Basic packet buffer info
 *
 */
struct PacketBuffer {
	std::byte* _data;
	size_t _len;
};

/**
 * @brief Struct that holds statistical information about a Output Queue.
 */
struct OutputQueueStats {
	/**
	 * @brief Updates the time-related statistics of the Output Queue.
	 *
	 * This function updates the time-related statistics of the Output Queue, such as the transmit
	 * start time and end time. It uses the CLOCK_MONOTONIC_COARSE clock to retrieve the current
	 * time and updates the transmit end time with the current time. If the transmit start time is
	 * uninitialized (zero), it is also set to the current time.
	 */
	void UpdateTime() noexcept;

	uint64_t transmittedPackets {}; /**< Number of transmitted packets. */
	uint64_t transmittedBytes {}; /**< Total size of transmitted data in bytes. */
	uint64_t failedPackets {}; /**< Number of transmit failed packets. */
	uint64_t upscaledPackets {}; /**< Number of upscaled packets to min size. */
	timespec transmitStartTime {}; /**< Start time of the transmit process. */
	timespec transmitEndTime {}; /**< End time of the transmit process. */
};

/**
 * @brief Output queue virtual interface
 *
 * First, the user must fill array of PacketBuffer structure with required
 * packets length and than call function GetBurst that assign memory of
 * requiered length to PacketBuffer _data variable. Assigned memory has to be
 * filled by user after calling GetBurst function with valid packet data.
 *
 * Function SendBurst has to be called immediately after GetBurst.
 * It is not possible to call multiple GetBurst without calling SendBurst,
 * an unexpected behaviour may occur
 */
class OutputQueue {
public:
	/**
	 * @brief Construct a new Output Queue object
	 *
	 * Initializes rate limiter to the default state.
	 */
	OutputQueue();

	/**
	 * @brief Set the rate limiter.
	 *
	 * This function sets the rate limiter configuration for the output queue. The rate limiter can
	 * be configured with a specified rate limit, which can be either in packets per second
	 * (RateLimitPps) or megabits per second (RateLimitMbps). The rate limit configuration is passed
	 * as a parameter of type Config::RateLimit.
	 *
	 * @param rateLimitConfig The rate limiter configuration to set.
	 *
	 * @throws std::logic_error If the rate limit config type is not implemented.
	 */
	void SetRateLimiter(Config::RateLimit rateLimitConfig);

	/**
	 * @brief Get the statistics of the Output Queue.
	 *
	 * @return OutputQueueStats object containing the statistical information.
	 */
	OutputQueueStats GetOutputQueueStats() const noexcept { return _outputQueueStats; }

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return maximal possible burst size
	 */
	virtual size_t GetMaxBurstSize() const noexcept = 0;

	/**
	 * @brief Get @p burstSize PacketBuffers
	 *
	 * Function expects filled variable @p _len with requiered packet length
	 * in PacketBuffer structure array.
	 * Function fill the @p _data pointers with memory that can hold @p _len
	 * bytes.
	 *
	 * After this function the user must fill the @p _data memory with valid
	 * packet data.
	 *
	 * @warning @p burstSize cannot be bigger than GetMaxBurstSize()
	 *
	 * @param[in,out] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of PacketBuffers
	 */
	virtual void GetBurst(PacketBuffer* burst, size_t burstSize) = 0;

	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] burst pointer to PacketBuffer array
	 */
	virtual void SendBurst(const PacketBuffer* burst) = 0;

	/**
	 * @brief Default virtual destructor
	 */
	virtual ~OutputQueue() = default;

protected:
	/**
	 * @brief Apply rate limiting.
	 *
	 * This function applies rate limiting by calculating the number of tokens required to send the
	 * specified number of packets or total length of packets, based on the rate limiting type set
	 * by the `SetRateLimiter` function. It then calls the `Limit` function of the `RateLimiter`
	 * object `_rateLimiter` to limit the sending rate.
	 *
	 * @param[in] packetsCount The number of packets to be sent.
	 * @param[in] sumOfPacketsLength The total length of all packets to be sent.
	 */
	void RateLimit(uint64_t packetsCount, uint64_t sumOfPacketsLength);

	OutputQueueStats _outputQueueStats;

private:
	Config::RateLimit _rateLimitType;
	RateLimiter _rateLimiter;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("OutputQueue");
};

} // namespace replay
