/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Pcap file plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapFile.hpp"

#include "outputPluginFactoryRegistrator.hpp"

#include <cstddef>
#include <ctime>
#include <map>

namespace replay {

// register pcap file plugin to the factory
OutputPluginFactoryRegistrator<PcapFilePlugin> pcapFilePluginRegistration("pcapFile");

PcapFileQueue::PcapFileQueue(const std::string& fileName, size_t pktSize, size_t burstSize)
	: _pktSize(pktSize)
	, _burstSize(burstSize)
	, _pktsToSend(0)
	, _buffer(std::make_unique<std::byte[]>(burstSize * pktSize))
{
	CreatePcapFile(fileName);
}

void PcapFileQueue::CreatePcapFile(const std::string& fileName)
{
	constexpr int maxSnaplen = 65535;
	_pcapHandler.reset(pcap_open_dead(DLT_EN10MB, maxSnaplen));
	if (_pcapHandler == nullptr) {
		_logger->error("pcap_open_dead() has failed");
		throw std::runtime_error("PcapFileQueue::CreatePcapFile() has failed");
	}
	_pcapDumper.reset(pcap_dump_open(_pcapHandler.get(), fileName.c_str()));
	if (_pcapDumper == nullptr) {
		_logger->error("Unable to open pcap file: " + fileName);
		throw std::runtime_error("PcapFileQueue::CreatePcapFile() has failed");
	}
}

void PcapFileQueue::WritePacket(const std::byte* packetData, size_t packetLength)
{
	auto packetHeader = CreatePacketHeader(packetLength);
	pcap_dump(
		reinterpret_cast<u_char*>(_pcapDumper.get()),
		&packetHeader,
		reinterpret_cast<const u_char*>(packetData));
}

struct pcap_pkthdr PcapFileQueue::CreatePacketHeader(unsigned int packetLength) const
{
	return {GetCurrentTime(), packetLength, packetLength};
}

struct timeval PcapFileQueue::GetCurrentTime() const
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv;
}

void PcapFileQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	if (_pktsToSend) {
		_logger->error(
			"GetBurst() called before the previous request was "
			"processed by SendBurst()");
		throw std::runtime_error("PcapFileQueue::GetBurst() has failed");
	}
	if (burstSize > _burstSize) {
		_logger->error(
			"Requested burstSize {} is bigger than predefiened {}",
			burstSize,
			_burstSize);
		throw std::runtime_error("PcapFileQueue::GetBurst() has failed");
	}

	for (size_t packetId = 0; packetId < burstSize; packetId++) {
		if (burst[packetId]._len > _pktSize) {
			_logger->error("Requested packet size {} is too big", burst[packetId]._len);
			throw std::runtime_error("PcapFileQueue::GetBurst() has failed");
		}
		burst[packetId]._data = &_buffer[packetId * _pktSize];
	}
	_pktsToSend = burstSize;
}

void PcapFileQueue::SendBurst(const PacketBuffer* burst)
{
	for (size_t packetId = 0; packetId < _pktsToSend; packetId++) {
		WritePacket(burst[packetId]._data, burst[packetId]._len);
	}
	_pktsToSend = 0;
}

size_t PcapFileQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

PcapFilePlugin::PcapFilePlugin(const std::string& params)
{
	ParseArguments(params);

	for (size_t queueId = 0; queueId < _queueCount; queueId++) {
		auto decoratedFileName = AppendQueueIdToFileName(queueId);
		_queues.emplace_back(
			std::make_unique<PcapFileQueue>(decoratedFileName, _packetSize, _burstSize));
	}
}

std::string PcapFilePlugin::AppendQueueIdToFileName(uint16_t queueId)
{
	// append queue id only in multi queue mode
	if (_queueCount == 1) {
		return _fileName;
	}

	return _fileName + "." + std::to_string(queueId);
}

size_t PcapFilePlugin::GetQueueCount() const noexcept
{
	return _queueCount;
}

OutputQueue* PcapFilePlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

void PcapFilePlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "file") {
			_fileName = value;
		} else if (key == "queueCount") {
			_queueCount = std::stoul(value);
		} else if (key == "packetSize") {
			_packetSize = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("PcapFilePlugin::ParseMap() has failed");
		}
	}
}

int PcapFilePlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error(
			"Parameter \"packetSize\", \"burstSize\" or \"queueCount\" has wrong format");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	if (_fileName.empty()) {
		_logger->error("Required parameter \"file\" missing/empty");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	if (!_queueCount) {
		_logger->error("Parameter \"queueCount\" has to be bigger than 0");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
