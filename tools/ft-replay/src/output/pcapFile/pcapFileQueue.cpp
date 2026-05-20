/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file queue implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapFileQueue.hpp"
#include "pcapFileWriter.hpp"

namespace replay {

PcapFileQueue::PcapFileQueue(
	const std::string& fileName,
	size_t pktSize,
	size_t burstSize,
	bool multiPort)
	: _pktSize(pktSize)
	, _burstSize(burstSize)
	, _pktsToSend(0)
	, _buffer(std::make_unique<std::byte[]>(burstSize * pktSize))
{
	if (multiPort) {
		_writers.emplace_back(fileName + ".A");
		_writers.emplace_back(fileName + ".B");
	} else {
		_writers.emplace_back(fileName);
	}
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
		auto& writer = GetOutputWriter(burst[packetId]._info);
		writer.WritePacket(burst[packetId]._data, burst[packetId]._len);
		_outputQueueStats.transmittedPackets++;
		_outputQueueStats.transmittedBytes += burst[packetId]._len;
		_outputQueueStats.UpdateTime();
	}
	_pktsToSend = 0;
}

PcapFileWriter& PcapFileQueue::GetOutputWriter(const PacketInfo* pktInfo)
{
	if (_writers.size() == 1) {
		return _writers.front();
	}

	return (pktInfo->outInterface == OutInterface::Interface0) ? _writers.front() : _writers.back();
}

size_t PcapFileQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

} // namespace replay
