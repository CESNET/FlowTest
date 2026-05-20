/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw queue interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "rawWriter.hpp"

#include "logger.h"
#include "outputQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace replay {

class RawQueue : public OutputQueue {
public:
	/**
	 * @brief Construct RawQueue
	 *
	 * Constructs RawQueue. RawQueue is used to send packets via RawSocket.
	 *
	 * @param[in] ifcNames names of interfaces
	 * @param[in] pktSize Maximal packet size
	 * @param[in] burstSize Number of packets buffer should hold
	 */
	RawQueue(const std::vector<std::string>& ifcNames, size_t pktSize, size_t burstSize);

	/**
	 * @brief Default destructor
	 */
	~RawQueue() = default;

	RawQueue(const RawQueue&) = delete;
	RawQueue(RawQueue&&) = delete;
	RawQueue& operator=(const RawQueue&) = delete;
	RawQueue& operator=(RawQueue&&) = delete;

	/**
	 * @brief Get buffers of desired size for packets
	 *
	 * Assigns buffers of size _len to _data
	 *
	 * @param[in,out] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of PacketBuffers
	 */
	void GetBurst(PacketBuffer* burst, size_t burstSize) override;

	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] burst pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	RawWriter& GetOutputWriter(const PacketInfo* pktInfo);

	size_t _pktSize;
	size_t _burstSize;
	size_t _pktsToSend;

	std::unique_ptr<std::byte[]> _buffer;
	std::vector<RawWriter> _writers;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawQueue");
};

} // namespace replay
