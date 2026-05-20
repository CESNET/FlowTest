/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file queue interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputQueue.hpp"
#include "pcapFileWriter.hpp"

namespace replay {

class PcapFileQueue : public OutputQueue {
public:
	/**
	 * @brief Construct PcapFileQueue
	 *
	 * @param[in] fileNames pcap fileNames to create
	 * @param[in] pktSize maximal packet size
	 * @param[in] burstSize of packets buffer should hold
	 */
	PcapFileQueue(const std::string& fileName, size_t pktSize, size_t burstSize, bool multiPort);

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
	PcapFileWriter& GetOutputWriter(const PacketInfo* pktInfo);

	size_t _pktSize;
	size_t _burstSize;
	size_t _pktsToSend;

	std::unique_ptr<std::byte[]> _buffer;
	std::vector<PcapFileWriter> _writers;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PcapFileQueue");
};

} // namespace replay
