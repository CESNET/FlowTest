/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Queue provider interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <pcap.h>

#include <memory>
#include <string>

namespace replay {

/**
 * @brief Represent extracted raw packet data.
 *
 */
struct RawPacket {
	const std::byte* data;
	uint16_t dataLen;
	uint64_t timestamp;
};

/**
 * @brief Provides RawPacket data from pcap file.
 *
 */
class RawPacketProvider {
public:
	/**
	 * @brief Open and validate pcap file
	 *
	 * @param file pcap filename
	 */
	explicit RawPacketProvider(const std::string& file);

	/**
	 * @brief Return pointer to the next valid packet (RawPacket) in pcap file.
	 *
	 * @return const struct RawPacket* Pointer to RawPacket data.
	 */
	const struct RawPacket* Next();

private:
	void OpenFile(const char* file);
	void CheckDatalink();
	bool IsHeaderValid(const struct pcap_pkthdr* header);
	void FillRawPacket(const struct pcap_pkthdr* header, const std::byte* pktData);
	uint64_t CalculateTimestamp(const struct pcap_pkthdr* header);

	struct RawPacket _packet;
	std::unique_ptr<pcap_t, decltype(&pcap_close)> _handler {nullptr, &pcap_close};
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawPacketProvider");
};

} // namespace replay
