/**
 * @file
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file writer interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <cstddef>
#include <memory>
#include <pcap.h>

namespace replay {

class PcapFileWriter {
public:
	/**
	 * @brief Construct PcapFileWriter
	 *
	 * @param[in] fileName name of the output pcap file
	 */
	PcapFileWriter(const std::string& fileName);

	/**
	 * @brief Write a packet to output pcap file
	 *
	 * @param[in] packetData pointer to packet bytes
	 * @param[in] packetLength length of the packet
	 */
	void WritePacket(const std::byte* packetData, size_t packetLength);

private:
	void CreatePcapFile(const std::string& fileName);

	struct pcap_pkthdr CreatePacketHeader(unsigned int packetLength) const;
	struct timeval GetCurrentTime() const;

	std::unique_ptr<pcap_t, decltype(&pcap_close)> _pcapHandler {nullptr, &pcap_close};
	std::unique_ptr<pcap_dumper_t, decltype(&pcap_dump_close)> _pcapDumper {
		nullptr,
		&pcap_dump_close};

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PcapFileWriter");
};

} // namespace replay
