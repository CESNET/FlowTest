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
	OutputQueue() = default;

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
	OutputQueueStats _outputQueueStats;

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("OutputQueue");
};

} // namespace replay
