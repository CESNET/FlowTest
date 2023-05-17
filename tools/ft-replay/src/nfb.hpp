/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief NFB plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "outputQueue.hpp"

extern "C" {
#include <libfdt.h>
#include <nfb/ndp.h>
#include <nfb/nfb.h>
}

#include <cstddef>
#include <memory>

namespace replay {

enum class SuperPackets {
	Auto,
	Enable,
	Disable,
};

class __attribute__((packed)) SuperPacketHeader final {
public:
	/**
	 * @brief Clear header
	 */
	void clear() noexcept;

	/**
	 * @brief Set packet length
	 *
	 * @param[in] length of packet
	 */
	void setLength(uint16_t length) noexcept;

	/**
	 * @brief Set next header flag
	 *
	 * @param[in] next header flag
	 */
	void setHasNextHeader(bool value) noexcept;

private:
	uint16_t _length : 15;
	uint16_t _hasNextHeader : 1;
	uint16_t _l2Len : 7;
	uint16_t _l3Len : 9;
	uint8_t _flags;
	uint8_t _timestamp[6];
	uint8_t _reserved[5];
};

static_assert(sizeof(SuperPacketHeader) == 16, "Invalid header definition");

class NfbQueue : public OutputQueue {
public:
	/**
	 * @brief Construct NfbQueue
	 *
	 * Constructs NfbQueue. NfbQueue is used to send packets via Nfb device.
	 *
	 * @param[in] pointer to nfb device
	 * @param[in] queue id
	 * @param[in] maximal size of burst
	 */
	NfbQueue(
		nfb_device* dev,
		unsigned int queue_id,
		size_t burstSize,
		size_t superPacketSize,
		size_t superPacketLimit);

	/**
	 * @brief Destructor
	 */
	~NfbQueue();

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
	 * @param[in] burst pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	void GetRegularBurst(PacketBuffer* burst, size_t burstSize);

	void GetSuperBurst(PacketBuffer* burst, size_t burstSize);

	void GetBuffers(size_t burstSize);

	size_t AlignBlockSize(size_t size);

	void Flush();

	std::unique_ptr<ndp_tx_queue_t, decltype(&ndp_close_tx_queue)> _txQueue {
		nullptr,
		&ndp_close_tx_queue};
	std::unique_ptr<ndp_packet[]> _txPacket;
	const size_t _maxBurstSize;
	size_t _superPacketSize;
	size_t _superPacketLimit;
	size_t _lastBurstTotalPacketLen;
	size_t _lastBurstSize;
	bool _isBufferInUse = false;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbQueue");
};

class NfbPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct NfbPlugin
	 *
	 * Constructs NfbPlugin and NfbQueue.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit NfbPlugin(const std::string& params);

	/**
	 * @brief Default destructor
	 */
	~NfbPlugin() = default;

	NfbPlugin(const NfbPlugin&) = delete;
	NfbPlugin(NfbPlugin&&) = delete;
	NfbPlugin& operator=(const NfbPlugin&) = delete;
	NfbPlugin& operator=(NfbPlugin&&) = delete;
	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

private:
	void DetermineSuperPacketSize();

	size_t GetSuperPacketLimit();

	int ParseArguments(const std::string& args);

	void ParseMap(const std::map<std::string, std::string>& argMap);

	std::unique_ptr<nfb_device, decltype(&nfb_close)> _nfbDevice {nullptr, &nfb_close};
	std::vector<std::unique_ptr<NfbQueue>> _queues;
	std::string _deviceName;
	size_t _queueCount = 0;
	size_t _burstSize = 64;
	size_t _superPacketSize = 2048;
	enum SuperPackets _superPackets = SuperPackets::Auto;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbPlugin");
};

OutputPluginFactoryRegistrator<NfbPlugin> nfbPluginRegistration("nfb");

} // namespace replay
