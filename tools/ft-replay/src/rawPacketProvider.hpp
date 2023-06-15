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
#include <optional>
#include <string>

namespace replay {

/**
 * @brief Represent extracted raw packet data.
 *
 */
struct RawPacket {
	const std::byte* data;
	uint16_t dataLen;
	uint64_t timestamp; ///< nanoseconds
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
	 * If @p normalizeTimestamp is enabled, the first packet will have zero timestamp
	 * and all remaining packets will have timestamp relative to the first packet.
	 * If the option is disabled, original packet timestamp from UNIX Epoch is preserved.
	 *
	 * @param file pcap filename
	 * @param normalizeTimestamp Normalize timestamps relative to the first packet.
	 */
	explicit RawPacketProvider(const std::string& file, bool normalizeTimestamps = true);

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
	uint64_t NormalizeTimestamp(uint64_t timestamp);

	struct RawPacket _packet;
	std::unique_ptr<pcap_t, decltype(&pcap_close)> _handler {nullptr, &pcap_close};

	bool _normalizeTimestamp;
	std::optional<uint64_t> _referenceTimestamp;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawPacketProvider");
};

} // namespace replay
