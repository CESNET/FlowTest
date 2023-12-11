/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Writing of generated packets to a PCAP output file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "timestamp.h"

#include <pcap/pcap.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

namespace generator {

/**
 * @brief A pcap writer class
 */
class PcapWriter {
public:
	/**
	 * @brief Construct a new pcap writer
	 *
	 * @param filename  The output pcap filename
	 *
	 * @throws runtime_error  When creation of the underlying pcap_t or pcap_dumper_t object failed
	 */
	explicit PcapWriter(const std::string& filename);

	// Disable copy and move constructors
	PcapWriter(const PcapWriter&) = delete;
	PcapWriter(PcapWriter&&) = delete;
	PcapWriter& operator=(const PcapWriter&) = delete;
	PcapWriter& operator=(PcapWriter&&) = delete;

	/**
	 * @brief Write a packet to the output file
	 *
	 * @param data  The packet data
	 * @param length  The packet length
	 * @param timestamp  The packet timestamp
	 *
	 * @throws runtime_error  When file write failed
	 */
	void WritePacket(const std::byte* data, uint32_t length, Timestamp timestamp);

private:
	std::unique_ptr<pcap_t, decltype(&pcap_close)> _pcap {nullptr, pcap_close};
	std::unique_ptr<pcap_dumper_t, decltype(&pcap_dump_close)> _dumper {nullptr, pcap_dump_close};
	std::FILE* _fp = 0;
};

} // namespace generator
