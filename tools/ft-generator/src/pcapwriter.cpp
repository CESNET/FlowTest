/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Writing of generated packets to a PCAP output file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapwriter.h"

#include <limits>
#include <stdexcept>

namespace generator {

PcapWriter::PcapWriter(const std::string& filename)
{
	_pcap.reset(pcap_open_dead(DLT_EN10MB, std::numeric_limits<uint16_t>::max()));
	if (!_pcap) {
		throw std::runtime_error("pcap open failed: " + std::string(pcap_geterr(_pcap.get())));
	}

	_dumper.reset(pcap_dump_open(_pcap.get(), filename.c_str()));
	if (!_dumper) {
		throw std::runtime_error("pcap dump open failed" + std::string(pcap_geterr(_pcap.get())));
	}
}

void PcapWriter::WritePacket(const std::byte* data, uint32_t length, timeval timestamp)
{
	pcap_pkthdr header;
	header.caplen = length;
	header.len = length;
	header.ts = timestamp;

	pcap_dump(
		reinterpret_cast<u_char*>(_dumper.get()),
		&header,
		reinterpret_cast<const u_char*>(data));
}

} // namespace generator
