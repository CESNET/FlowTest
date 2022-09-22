/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "socketDescriptor.hpp"
#include "outputQueue.hpp"
#include "outputPlugin.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "logger.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <linux/if_packet.h>

namespace replay {

class RawQueue : public OutputQueue {
public:
	/**
	 * @brief Construct RawOutputQueue
	 *
	 * Constructs RawOutputQueue. RawOutputQueue is used to send packets via RawSocket.
	 *
	 * @param[in] IFC name
	 * @param[in] Number of packets buffer should hold
	 * @param[in] Maximal size of packet
	 */
	RawQueue(const std::string& ifcName, const size_t& pktSize, const size_t& bufferSize);
	/**
	 * @brief Default destructor
	 */
	~RawQueue() = default;

	RawQueue() = delete;
	RawQueue(const RawQueue&) = delete;
	RawQueue(RawQueue&&) = delete;
	RawQueue& operator=(const RawQueue&) = delete;
	RawQueue& operator=(RawQueue&&) = delete;
	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] pointer to PacketBuffer array
	 * @param[in] number of packets to send
	 */
	void SendBurst(const PacketBuffer* burst, size_t burstSize) override;

	/**
	 * @brief Get buffers of desired size for packets
	 *
	 * Assigns buffers of size _len to _data
	 *
	 * @param[in,out] pointer to PacketBuffer array
	 * @param[in] number of PacketBuffers
	 *
	 * @return number of assigned buffers
	 */
	size_t GetBurst(PacketBuffer* burst, size_t burstSize) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	size_t _pktSize;
	size_t _burstSize;
	SocketDescriptor _socket;
	struct sockaddr_ll _sockAddr = {};
	std::unique_ptr<std::byte[]> _buffer;
	bool _bufferFlag = false;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawQueue");
};

class RawPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct RawPlugin
	 *
	 * Constructs RawPlugin, RawOutputQueue and RawBuffer.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit RawPlugin(const std::string& params);
	/**
	 * @brief Default destructor
	 */
	~RawPlugin() = default;

	RawPlugin() = delete;
	RawPlugin(const RawPlugin&) = delete;
	RawPlugin(RawPlugin&&) = delete;
	RawPlugin& operator=(const RawPlugin&) = delete;
	RawPlugin& operator=(RawPlugin&&) = delete;
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
	/**
	 * @brief Parse arguments in map
	 *
	 * @param[in] map with arguments
	 */
	void ParseMap(const std::map<std::string, std::string>& argMap);
	/**
	 * @brief Parse arguments
	 *
	 * Example format : arg1=value1,arg2=value2,...
	 *
	 * @param[in] string with arguments, separated by comma
	 *
	 * @return pointer to OutputQueue
	 */
	int ParseArguments(const std::string& args);

	std::unique_ptr<RawQueue> _queue;
	std::string _ifcName;
	size_t _burstSize = 1024;
	size_t _packetSize = 2048;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawPlugin");
};

OutputPluginFactoryRegistrator<RawPlugin> rawPluginRegistration("raw");

} // namespace replay
