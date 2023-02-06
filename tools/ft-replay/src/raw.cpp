/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "raw.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <linux/if_packet.h>
#include <map>
#include <memory>
#include <net/if.h>
#include <sys/ioctl.h>

namespace replay {

RawQueue::RawQueue(const std::string& ifcName, size_t pktSize, size_t burstSize)
	: _pktSize(pktSize)
	, _burstSize(burstSize)
	, _pktsToSend(0)
	, _buffer(std::make_unique<std::byte[]>(burstSize * pktSize))
{
	_socket.OpenSocket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);

	ifreq ifReq = {};
	snprintf(ifReq.ifr_name, IFNAMSIZ, "%s", ifcName.c_str());

	if ((ioctl(_socket.GetSocketId(), SIOCGIFINDEX, &ifReq)) < 0) {
		_logger->error("Cannot obtain interface id");
		throw std::runtime_error("RawQueue::RawQueue() has failed");
	}

	_sockAddr.sll_ifindex = ifReq.ifr_ifindex;
}

void RawQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	if (_bufferFlag) {
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
	_bufferFlag = true;
	_pktsToSend = burstSize;
}

void RawQueue::SendBurst(const PacketBuffer* burst)
{
	for (unsigned i = 0; i < _pktsToSend; i++) {
		while (true) {
			int ret = sendto(
				_socket.GetSocketId(),
				burst[i]._data,
				burst[i]._len,
				0,
				(const sockaddr*) &_sockAddr,
				sizeof(sockaddr_ll));

			if (ret == static_cast<int>(burst[i]._len)) {
				break;
			}

			if (ret == -1 && errno == EINTR) {
				continue;
			}
			_logger->error("RawQueue::SendBurst() Error while sending, errno: {} ", errno);
			break;
		}
	}
	_bufferFlag = false;
}

size_t RawQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

RawPlugin::RawPlugin(const std::string& params)
{
	ParseArguments(params);
	_queue = std::make_unique<RawQueue>(_ifcName, _packetSize, _burstSize);
}

size_t RawPlugin::GetQueueCount() const noexcept
{
	return 1;
}

OutputQueue* RawPlugin::GetQueue(uint16_t queueId)
{
	if (queueId != 0 || _queue == nullptr) {
		_logger->error("Invalid request for OutputQueue");
		throw std::runtime_error("RawPlugin::GetQueue() has failed");
	}
	return _queue.get();
}

void RawPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "ifc") {
			_ifcName = value;
		} else if (key == "packetSize") {
			_packetSize = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("RawPlugin::ParseMap() has failed");
		}
	}
}

int RawPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error("Parameter \"packetSize\" or \"burstSize\" wrong format");
		throw std::invalid_argument("RawPlugin::ParseArguments() has failed");
	}

	if (_ifcName.empty()) {
		_logger->error("Required parameter \"ifc\" missing/empty");
		throw std::invalid_argument("RawPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
