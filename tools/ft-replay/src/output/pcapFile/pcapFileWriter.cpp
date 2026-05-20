/**
 * @file
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file writer implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapFileWriter.hpp"

namespace replay {

PcapFileWriter::PcapFileWriter(const std::string& fileName)
{
	CreatePcapFile(fileName);
}

void PcapFileWriter::CreatePcapFile(const std::string& fileName)
{
	constexpr int maxSnaplen = 65535;
	_pcapHandler.reset(pcap_open_dead(DLT_EN10MB, maxSnaplen));
	if (_pcapHandler == nullptr) {
		_logger->error("pcap_open_dead() has failed");
		throw std::runtime_error("PcapFileWriter::CreatePcapFile() has failed");
	}
	_pcapDumper.reset(pcap_dump_open(_pcapHandler.get(), fileName.c_str()));
	if (_pcapDumper == nullptr) {
		_logger->error("Unable to open pcap file: " + fileName);
		throw std::runtime_error("PcapFileWriter::CreatePcapFile() has failed");
	}
}

void PcapFileWriter::WritePacket(const std::byte* packetData, size_t packetLength)
{
	auto packetHeader = CreatePacketHeader(packetLength);
	pcap_dump(
		reinterpret_cast<u_char*>(_pcapDumper.get()),
		&packetHeader,
		reinterpret_cast<const u_char*>(packetData));
}

struct pcap_pkthdr PcapFileWriter::CreatePacketHeader(unsigned int packetLength) const
{
	return {GetCurrentTime(), packetLength, packetLength};
}

struct timeval PcapFileWriter::GetCurrentTime() const
{
	struct timeval tv;
	int ret = gettimeofday(&tv, NULL);

	if (ret < 0) {
		_logger->error("gettimeofday() failed!");
		throw std::runtime_error("PcapFileWriter::GetCurrentTime() has failed");
	}

	return tv;
}

} // namespace replay
