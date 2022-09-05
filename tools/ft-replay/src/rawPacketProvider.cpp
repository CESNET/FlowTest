/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Queue provider implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rawPacketProvider.hpp"

#include <stdexcept>
#include <string>

namespace replay {

RawPacketProvider::RawPacketProvider(const std::string& file)
	: _packet()
{
	OpenFile(file.c_str());
	CheckDatalink();
}

void RawPacketProvider::OpenFile(const char *file)
{
	char errbuff[PCAP_ERRBUF_SIZE];
	_handler.reset(pcap_open_offline_with_tstamp_precision(
		file,
		PCAP_TSTAMP_PRECISION_MICRO,
		errbuff));

	if (_handler == nullptr) {
		throw std::runtime_error("Unable to open pcap file: " + std::string(errbuff));
	}
}

void RawPacketProvider::CheckDatalink()
{
	if (pcap_datalink(_handler.get()) != DLT_EN10MB) {
		throw std::runtime_error("Unsupported link layer protocol! Only DLT_EN10MB supported.");
	}
}

const struct RawPacket *RawPacketProvider::Next()
{
	struct pcap_pkthdr *header;
	const u_char *pktData;

	switch (pcap_next_ex(_handler.get(), &header, &pktData)) {
	case 1:
		if (IsHeaderValid(header)) {
			FillRawPacket(header, reinterpret_cast<const std::byte*>(pktData));
			return &_packet;
		} else {
			return Next();
		}
	case PCAP_ERROR:
		throw std::runtime_error(pcap_geterr(_handler.get()));
	case PCAP_ERROR_BREAK:
		return nullptr;
	default:
		throw std::runtime_error("Unexpected error, while reading pcap_file");
	}
}

bool RawPacketProvider::IsHeaderValid(const struct pcap_pkthdr *header)
{
	if (header->caplen > header->len) {
		_logger->info("Packet caplen differs from packet length!");
		return false;
	}

	return true;
}

void RawPacketProvider::FillRawPacket(const struct pcap_pkthdr *header, const std::byte *pktData)
{
	_packet.data = pktData;
	_packet.dataLen = header->caplen;
	_packet.timestamp = CalculateTimestamp(header);
}

uint64_t RawPacketProvider::CalculateTimestamp(const struct pcap_pkthdr *header)
{
	return (header->ts.tv_sec * 1000000ULL + header->ts.tv_usec) * 1000;
}

} // namespace replay
