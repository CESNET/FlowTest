/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbQueue class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "nfbReplicatorFirmware.hpp"
#include "offloads.hpp"
#include "outputQueue.hpp"

extern "C" {
#include <nfb/ndp.h>
#include <nfb/nfb.h>
}

#include <cstddef>
#include <memory>

namespace replay {

/**
 * @brief Configuration structure for NfbQueue.
 */
struct NfbQueueConfig {
	size_t maxBurstSize = 64;
	size_t maxPacketSize;
	size_t superPacketLimit;
	bool superPacketsEnabled = false;
	bool replicatorHeaderEnabled = false;
};

/**
 * @brief Represents a queue for transmitting packets using NDP.
 *
 * The NfbQueue class inherits from OutputQueue and provides methods for transmitting packets
 * using the NDP.
 */
class NfbQueue : public OutputQueue {
public:
	/**
	 * @brief Constructor for NfbQueue.
	 * @param queueConfig Configuration for the NfbQueue.
	 * @param dev Pointer to the nfb device.
	 * @param queueId ID of the queue.
	 */
	NfbQueue(const NfbQueueConfig& queueConfig, nfb_device* dev, unsigned queueId);

	/**
	 * @brief Destructor for NfbQueue.
	 */
	~NfbQueue() override;

	NfbQueue(const NfbQueue&) = delete;
	NfbQueue(NfbQueue&&) = delete;
	NfbQueue& operator=(const NfbQueue&) = delete;
	NfbQueue& operator=(NfbQueue&&) = delete;

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
	 * @param[in] pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

	/**
	 * @brief Sets the offloads for the queue.
	 * @param offloads The offloads to set.
	 */
	void SetOffloads(Offloads offloads) noexcept;

	/**
	 * @brief Flush output buffer
	 */
	void Flush() override;

private:
	void SetupNdpQueue(unsigned queueId);

	void GetRegularBurst(PacketBuffer* burst, size_t burstSize);
	void GetSuperBurst(PacketBuffer* burst, size_t burstSize);
	void GetBuffers(size_t burstSize);
	void FlushBuffers();

	void FillReplicatorHeader(const PacketBuffer& packetBuffer, NfbReplicatorHeader& header);
	size_t AlignBlockSize(size_t size);

	std::unique_ptr<ndp_tx_queue_t, decltype(&ndp_close_tx_queue)> _txQueue {
		nullptr,
		&ndp_close_tx_queue};
	std::unique_ptr<ndp_packet[]> _txPacket;

	NfbQueueConfig _queueConfig;
	nfb_device* _nfbDevice;

	size_t _lastBurstTotalPacketLen;
	size_t _lastBurstSize;
	Offloads _offloads;
	bool _isBufferInUse;

	static constexpr size_t HEADER_LEN = sizeof(NfbReplicatorHeader);
	static constexpr size_t MIN_PACKET_SIZE = 60;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbQueue");
};

} // namespace replay
