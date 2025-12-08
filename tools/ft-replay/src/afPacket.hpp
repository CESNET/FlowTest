/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Output plugin for AF_PACKET interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "memoryMapping.hpp"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"
#include "socketDescriptor.hpp"

#include <memory>
#include <string>
#include <vector>

namespace replay {

/**
 * @brief Configuration parameters of the plugin
 */
struct AfPacketConfig {
	std::string _ifcName;
	size_t _queueCount = 0;
	size_t _burstSize = 32;

	size_t _blockSize = 16384;
	size_t _frameSize = 2048; // a.k.a. packetSize
	size_t _frameCount = 1024;
	bool _qdiskBypass = true;
	bool _packetLoss = false;
};

/**
 * @brief AF_PACKET output queue
 *
 * The AfPacketQueue class inherits from OutputQueue and provides methods for transmitting packets
 * using AF_PACKET socket.
 */
class AfPacketQueue : public OutputQueue {
public:
	/**
	 * @brief Construct AF_PACKET queue
	 * @param[in] cfg     Plugin configuration
	 * @param[in] queueId Identification of the queue
	 */
	AfPacketQueue(const AfPacketConfig& cfg, size_t queueId);
	/** @brief Class destructor */
	~AfPacketQueue();

	// Disable copy and move constructors
	AfPacketQueue(const AfPacketQueue&) = delete;
	AfPacketQueue(AfPacketQueue&&) = delete;
	AfPacketQueue& operator=(const AfPacketQueue&) = delete;
	AfPacketQueue& operator=(AfPacketQueue&&) = delete;

	/**
	 * @brief Get buffers of desired size for packets.
	 *
	 * Assigns buffers of size _len to _data.
	 * @param[in,out] burst Pointer to PacketBuffer array
	 * @param[in] burstSize Number of PacketBuffers
	 */
	void GetBurst(PacketBuffer* burst, size_t burstSize) override;
	/**
	 * @brief Send burst of packets.
	 * @param[in] pointer Pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;
	/**
	 * @brief Get the Maximal Burst Size
	 * @return Maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	void SocketSetup();
	void SocketSetupPacketVersion();
	void SocketSetupQdiskBypass();
	void SocketSetupPacketLoss();
	void SocketSetupTxRing();
	void SocketSetupBinding();
	void MemorySetup();

	int GetInterfaceIndex(int fd, std::string_view ifc);
	std::byte* GetFrame(size_t index);
	void TxPoll();

	AfPacketConfig _config;
	SocketDescriptor _socket;
	MemoryMapping _mapping;
	size_t _queueId;

	size_t _ringIndex = 0; // Index of the next unsent packet
	size_t _lastBurstSize = 0;
	size_t _lastBurstTotalPacketLen = 0;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("AfPacketQueue");
};

/**
 * @brief AF_PACKET output plugin
 */
class AfPacketPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct AF_PACKET plugin
	 * @param[in] args Command line arguments in form: "arg1=value1,arg2=value2"
	 */
	explicit AfPacketPlugin(const std::string& params);
	/** @brief Class destructor */
	~AfPacketPlugin() = default;

	// Disable copy and move constructors
	AfPacketPlugin(const AfPacketPlugin&) = delete;
	AfPacketPlugin(AfPacketPlugin&&) = delete;
	AfPacketPlugin& operator=(const AfPacketPlugin&) = delete;
	AfPacketPlugin& operator=(AfPacketPlugin&&) = delete;

	/** @brief Get queue count. */
	size_t GetQueueCount() const noexcept override;
	/**
	 * @brief Get pointer to ID-specific OutputQueue.
	 * @param[in] queueId Has to be in range of 0 - GetQueueCount()-1
	 * @return Pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;
	/** @brief Get MTU of the AF_PACKET interface. */
	size_t GetMTU() const noexcept override;

	/**
	 * @brief Get NUMA node to which the NIC is connected
	 *
	 * @return ID of NUMA or nullopt
	 */
	NumaNode GetNumaNode() override;

private:
	void PrintSettings();
	void ParseMap(const std::map<std::string, std::string>& argMap);
	int ParseArguments(const std::string& args);
	void DetermineMaxPacketSize();

	AfPacketConfig _config;
	size_t _maxPacketSize;
	std::vector<std::unique_ptr<AfPacketQueue>> _queues;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("AfPacketPlugin");
};

} // namespace replay
