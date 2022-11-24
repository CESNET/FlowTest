/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief NFB plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "nfb.hpp"

#include <nfb/nfb.h>
#include <nfb/ndp.h>

#include <map>
#include <memory>
#include <cstddef>

namespace replay {

NfbQueue::NfbQueue(nfb_device *dev, unsigned int queue_id, size_t burstSize):
	_txPacket(std::make_unique<ndp_packet[]>(burstSize)),
	_burstSize(burstSize)
{
	_txQueue.reset(ndp_open_tx_queue(dev, queue_id));
	if (!_txQueue) {
		_logger->error("Unable to open NDP TX queue: {}", queue_id);
		throw std::runtime_error("NfbQueue::NfbQueue() has failed");
	}
	if (ndp_queue_start(_txQueue.get()) != 0) {
		_logger->error("Unable to start NDP TX queue: {}", queue_id);
		throw std::runtime_error("NfbQueue::NfbQueue() has failed");
	}
}

NfbQueue::~NfbQueue() {
	Flush();
}

size_t NfbQueue::GetBurst(PacketBuffer* burst, size_t burstSize) {
	if (_txBurstCount != 0) {
		_logger->error("GetBurst() called before the previous request was processed by SendBurst()");
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}
	if (_burstSize < burstSize) {
		_logger->warn("Requested burstSize {} is bigger than predefined {}", burstSize, _burstSize);
		burstSize = _burstSize;
	}

	return GetRegularBurst(burst, burstSize);
}

size_t NfbQueue::GetRegularBurst(PacketBuffer* burst, size_t burstSize) {
	static constexpr size_t minPacketSize = 64;

	for (unsigned i = 0; i < burstSize; i++) {
		if (burst[i]._len < minPacketSize) {
			_txPacket[i].data_length = minPacketSize;
		} else {
			_txPacket[i].data_length = burst[i]._len;
		}
	}

	_txBurstCount = GetBuffers(burstSize);

	for (unsigned i = 0; i < _txBurstCount; i++) {
		burst[i]._data = reinterpret_cast<std::byte *>(_txPacket[i].data);
		if (_txPacket[i].data_length != burst[i]._len) {
			std::fill_n(_txPacket[i].data + burst[i]._len, _txPacket[i].data_length - burst[i]._len, 0);
		}
	}

	return _txBurstCount;
}

unsigned NfbQueue::GetBuffers(size_t burstSize) {
	unsigned ret = ndp_tx_burst_get(_txQueue.get(), _txPacket.get(), burstSize);
	if (ret == 0) {
		Flush();
		while (ret == 0) {
			ret = ndp_tx_burst_get(_txQueue.get(), _txPacket.get(), burstSize);
		}
	}

	return ret;
}

void NfbQueue::SendBurst(const PacketBuffer* burst, size_t burstSize) {
	if (burstSize != _txBurstCount) {
		_logger->error("Invalid burstSize. Provided {}, requested {}", _txBurstCount, burstSize);
		throw std::runtime_error("NfbQueue::SendBurst() has failed");
	}
	(void) burst;
	(void) burstSize;
	ndp_tx_burst_put(_txQueue.get());
	_txBurstCount = 0;
}

void NfbQueue::Flush() {
	ndp_tx_burst_flush(_txQueue.get());
	_txBurstCount = 0;
}

size_t NfbQueue::GetMaxBurstSize() const noexcept {
	return _burstSize;
}

NfbPlugin::NfbPlugin(const std::string& params) {
	ParseArguments(params);
	_nfbDevice.reset(nfb_open(_deviceName.c_str()));

	if (!_nfbDevice) {
		_logger->error("Unable to open NFB device \"{}\"", _deviceName);
		throw std::runtime_error("NfbPlugin::NfbPlugin() has failed");
	}

	if (_queueCount == 0) {
		int ret = ndp_get_tx_queue_count(_nfbDevice.get());
		if (ret <= 0) {
			_logger->error("ndp_get_tx_queue_count() has failed, returned: {}", ret);
			throw std::runtime_error("NfbPlugin::NfbPlugin() has failed");
		}
		_queueCount = static_cast<size_t>(ret);
	}

	for (size_t id = 0; id < _queueCount; id++) {
		_queues.emplace_back(std::make_unique<NfbQueue>(_nfbDevice.get(), id, _burstSize));
	}
}

int NfbPlugin::ParseArguments(const std::string& args) {
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error("Parameter \"queueCount\" or \"burstSize\" wrong format");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}

	if (_deviceName.empty()) {
		_logger->error("Required parameter \"device\" missing/empty");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

void NfbPlugin::ParseMap(const std::map<std::string, std::string>& argMap) {
	for (const auto& [key, value]: argMap) {
		if (key == "device") {
			_deviceName = value;
		} else if (key == "queueCount") {
			_queueCount = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("NfbPlugin::ParseMap() has failed");
		}
	}
}

size_t NfbPlugin::GetQueueCount() const noexcept {
	return _queueCount;
}

OutputQueue* NfbPlugin::GetQueue(uint16_t queueId) {
	return _queues.at(queueId).get();
}

} // namespace replay
