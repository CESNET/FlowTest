/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "outputQueue.hpp"
#include "socketDescriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <linux/if_packet.h>
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
	 * @param[in] IFC name
	 * @param[in] Maximal packet size
	 * @param[in] Number of packets buffer should hold
	 */
	RawQueue(const std::string& ifcName, size_t pktSize, size_t burstSize);
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
	 * @param[in] pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	size_t _pktSize;
	size_t _burstSize;
	size_t _pktsToSend;
	SocketDescriptor _socket;
	sockaddr_ll _sockAddr = {};
	std::unique_ptr<std::byte[]> _buffer;
	bool _bufferFlag = false;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawQueue");
};

class RawPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct RawPlugin
	 *
	 * Constructs RawPlugin and RawQueue.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit RawPlugin(const std::string& params);

	/**
	 * @brief Default destructor
	 */
	~RawPlugin() = default;

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
	 * @brief Get MTU of the raw interface
	 */
	size_t GetMTU() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

	/**
	 * @brief Get NUMA node to which the NIC is connected
	 *
	 * @return ID of NUMA or nullopt
	 */
	NumaNode GetNumaNode() override;

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

	void DeterminePacketSize();

	std::unique_ptr<RawQueue> _queue;
	std::string _ifcName;
	size_t _burstSize = 1024;
	size_t _packetSize = 0;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawPlugin");
};

OutputPluginFactoryRegistrator<RawPlugin> rawPluginRegistration("raw");

} // namespace replay
