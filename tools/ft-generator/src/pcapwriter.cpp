/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Writing of generated packets to a PCAP output file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapwriter.h"

#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace generator {

PcapWriter::PcapWriter(const std::string& filename)
{
	// We need to use pcap_dump_fopen to be able to check for write errors.
	// Unfortunately pcap_dump does not return any value to indicate errors, so we have to check the
	// FILE* directly to detect write errors.
	// fopen is done by us, fclose is done by pcap_dump_close.
	std::unique_ptr<std::FILE, decltype(&std::fclose)> fp(
		std::fopen(filename.c_str(), "wb"),
		&std::fclose);
	if (!fp) {
		throw std::runtime_error("file open failed: " + std::string(std::strerror(errno)));
	}

	_pcap.reset(pcap_open_dead_with_tstamp_precision(
		DLT_EN10MB,
		std::numeric_limits<uint16_t>::max(),
		PCAP_TSTAMP_PRECISION_NANO));
	if (!_pcap) {
		throw std::runtime_error("pcap open failed: " + std::string(pcap_geterr(_pcap.get())));
	}

	_dumper.reset(pcap_dump_fopen(_pcap.get(), fp.get()));
	if (!_dumper) {
		throw std::runtime_error("pcap dump open failed" + std::string(pcap_geterr(_pcap.get())));
	}
	_fp = fp.release();
}

void PcapWriter::WritePacket(const std::byte* data, uint32_t length, ft::Timestamp timestamp)
{
	pcap_pkthdr header;
	header.caplen = length;
	header.len = length;
	header.ts = {timestamp.SecPart(), timestamp.NanosecPart()};

	pcap_dump(
		reinterpret_cast<u_char*>(_dumper.get()),
		&header,
		reinterpret_cast<const u_char*>(data));
	if (std::ferror(_fp) != 0) {
		throw std::runtime_error(
			"failure writing packet to pcap file: " + std::string(std::strerror(errno)));
	}
}

} // namespace generator
