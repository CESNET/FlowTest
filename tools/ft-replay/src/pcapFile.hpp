/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Pcap file plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"

#include <cstddef>
#include <memory>

#include <pcap.h>

namespace replay {
class PcapFileQueue : public OutputQueue {
public:
	/**
	 * @brief Construct PcapFileQueue
	 *
	 * Constructs PcapFileQueue. PcapFileQueue is used to save packets via pcap file.
	 *
	 * @param[in] fileName pcap fileName to create
	 * @param[in] pktSize maximal packet size
	 * @param[in] burstSize of packets buffer should hold
	 */
	PcapFileQueue(const std::string& fileName, size_t pktSize, size_t burstSize);

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
	void CreatePcapFile(const std::string& fileName);
	void WritePacket(const std::byte* packetData, size_t packetLength);

	struct pcap_pkthdr CreatePacketHeader(unsigned int packetLength) const;
	struct timeval GetCurrentTime() const;

	size_t _pktSize;
	size_t _burstSize;
	size_t _pktsToSend;
	std::unique_ptr<std::byte[]> _buffer;

	std::unique_ptr<pcap_dumper_t, decltype(&pcap_dump_close)> _pcapDumper {
		nullptr,
		&pcap_dump_close};
	std::unique_ptr<pcap_t, decltype(&pcap_close)> _pcapHandler {nullptr, &pcap_close};

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PcapFileQueue");
};

class PcapFilePlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct PcapFilePlugin
	 *
	 * Constructs PcapFilePlugin and PcapFileQueues.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit PcapFilePlugin(const std::string& params);

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get MTU of the pcapFile interface
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

private:
	int ParseArguments(const std::string& args);
	void ParseMap(const std::map<std::string, std::string>& argMap);
	std::string AppendQueueIdToFileName(uint16_t queueId);

	std::vector<std::unique_ptr<PcapFileQueue>> _queues;
	std::string _fileName;
	size_t _burstSize = 1024;
	size_t _packetSize = 2048;
	size_t _queueCount = 1;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PcapFilePlugin");
};

} // namespace replay
