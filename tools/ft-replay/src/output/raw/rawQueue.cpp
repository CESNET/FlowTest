/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw queue implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rawQueue.hpp"

#include <cerrno>
#include <memory>

namespace replay {

RawQueue::RawQueue(const std::vector<std::string>& ifcNames, size_t pktSize, size_t burstSize)
	: _pktSize(pktSize)
	, _burstSize(burstSize)
	, _pktsToSend(0)
	, _buffer(std::make_unique<std::byte[]>(burstSize * pktSize))
{
	if (ifcNames.size() > 2 || ifcNames.empty()) {
		_logger->error(
			"The number of output interfaces must be 1 or 2. {} specified",
			ifcNames.size());
		throw std::runtime_error("RawQueue::RawQueue() has failed");
	}

	for (const std::string& ifcName : ifcNames) {
		_writers.emplace_back(ifcName);
	}
}

void RawQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	if (_pktsToSend != 0) {
		_logger->error(
			"GetBurst() called before the previous request was "
			"processed by SendBurst()");
		throw std::runtime_error("RawQueue::GetBurst() has failed");
	}
	if (burstSize > _burstSize) {
		_logger->error(
			"Requested burstSize {} is bigger than predefiened {}",
			burstSize,
			_burstSize);
		throw std::runtime_error("RawQueue::GetBurst() has failed");
	}

	for (unsigned i = 0; i < burstSize; i++) {
		if (burst[i]._len > _pktSize) {
			_logger->error("Requested packet size {} is too big", burst[i]._len);
			throw std::runtime_error("RawQueue::GetBurst() has failed");
		}
		burst[i]._data = &_buffer[i * _pktSize];
	}
	_pktsToSend = burstSize;
}

void RawQueue::SendBurst(const PacketBuffer* burst)
{
	for (unsigned i = 0; i < _pktsToSend; i++) {
		while (true) {
			auto& writer = GetOutputWriter(burst[i]._info);
			int ret = writer.SendPacket(burst[i]);

			if (ret == static_cast<int>(burst[i]._len)) {
				_outputQueueStats.transmittedPackets++;
				_outputQueueStats.transmittedBytes += burst[i]._len;
				break;
			}

			if (ret == -1 && errno == EINTR) {
				continue;
			}
			_outputQueueStats.failedPackets++;
			_logger->error("RawQueue::SendBurst() Error while sending, errno: {} ", errno);
			break;
		}
		_outputQueueStats.UpdateTime();
	}

	_pktsToSend = 0;
}

RawWriter& RawQueue::GetOutputWriter(const PacketInfo* pktInfo)
{
	if (_writers.size() == 1) {
		return _writers.front();
	}

	return (pktInfo->outInterface == OutInterface::Interface0) ? _writers.front() : _writers.back();
}

size_t RawQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

} // namespace replay
