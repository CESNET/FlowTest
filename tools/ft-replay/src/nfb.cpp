/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief NFB plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "nfb.hpp"

#include <nfb/ndp.h>
#include <nfb/nfb.h>

#include <cstddef>
#include <map>
#include <memory>

namespace replay {

void SuperPacketHeader::clear() noexcept
{
	std::memset(this, 0, sizeof(*this));
}

void SuperPacketHeader::setLength(uint16_t length) noexcept
{
	_length = htole16(length);
}

void SuperPacketHeader::setHasNextHeader(bool value) noexcept
{
	_hasNextHeader = !!value;
}

NfbQueue::NfbQueue(nfb_device* dev, unsigned int queue_id, size_t burstSize, size_t superPacketSize)
	: _txPacket(std::make_unique<ndp_packet[]>(burstSize))
	, _burstSize(burstSize)
	, _superPacketSize(superPacketSize)
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

NfbQueue::~NfbQueue()
{
	Flush();
}

void NfbQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	if (_isBufferInUse) {
		_logger->error(
			"GetBurst() called before the previous request was "
			"processed by SendBurst()");
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}
	if (burstSize > _burstSize) {
		_logger->error(
			"Requested burstSize {} is bigger than predefined {}",
			burstSize,
			_burstSize);
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}

	if (_superPacketSize) {
		GetSuperBurst(burst, burstSize);
	} else {
		GetRegularBurst(burst, burstSize);
	}

	_isBufferInUse = true;
}

void NfbQueue::GetRegularBurst(PacketBuffer* burst, size_t burstSize)
{
	static constexpr size_t minPacketSize = 64;

	for (unsigned i = 0; i < burstSize; i++) {
		if (burst[i]._len < minPacketSize) {
			_txPacket[i].data_length = minPacketSize;
		} else {
			_txPacket[i].data_length = burst[i]._len;
		}
	}

	GetBuffers(burstSize);

	for (unsigned i = 0; i < burstSize; i++) {
		burst[i]._data = reinterpret_cast<std::byte*>(_txPacket[i].data);
		if (_txPacket[i].data_length != burst[i]._len) {
			std::fill_n(
				_txPacket[i].data + burst[i]._len,
				_txPacket[i].data_length - burst[i]._len,
				0);
		}
	}
}

void NfbQueue::GetSuperBurst(PacketBuffer* burst, size_t burstSize)
{
	static constexpr size_t headerLen = sizeof(SuperPacketHeader);
	static constexpr size_t minPacketSize = 64;

	// fill _txPacket data_len
	unsigned txSuperPacketIndex = 0;
	_txPacket[0].data_length = 0;
	for (unsigned i = 0; i < burstSize; i++) {
		size_t nextSize = headerLen + std::max(burst[i]._len, minPacketSize);
		nextSize = AlignBlockSize(nextSize);

		if (_txPacket[txSuperPacketIndex].data_length + nextSize <= _superPacketSize) {
			_txPacket[txSuperPacketIndex].data_length += nextSize;
		} else {
			_txPacket[++txSuperPacketIndex].data_length = nextSize;
		}
	}
	txSuperPacketIndex++;

	GetBuffers(txSuperPacketIndex);

	// assign buffers
	txSuperPacketIndex = 0;
	unsigned i, pos = 0;
	for (i = 0; i < burstSize; i++) {
		size_t pktLen = std::max(burst[i]._len, minPacketSize);
		size_t nextSize = headerLen + pktLen;
		nextSize = AlignBlockSize(nextSize);
		// next packet/break condition
		if (pos + nextSize > _txPacket[txSuperPacketIndex].data_length) {
			txSuperPacketIndex++;
			pos = 0;
		}
		// fill header
		SuperPacketHeader* hdrPtr
			= reinterpret_cast<SuperPacketHeader*>(_txPacket[txSuperPacketIndex].data + pos);
		hdrPtr->clear();
		hdrPtr->setLength(pktLen);
		if (pos + nextSize >= _txPacket[txSuperPacketIndex].data_length) {
			hdrPtr->setHasNextHeader(false);
		} else {
			hdrPtr->setHasNextHeader(true);
		}

		pos += headerLen;

		// assign buffer
		burst[i]._data = reinterpret_cast<std::byte*>(_txPacket[txSuperPacketIndex].data + pos);
		// fill padding with zeros
		if (pktLen != burst[i]._len) {
			std::fill_n(
				_txPacket[txSuperPacketIndex].data + pos + burst[i]._len,
				pktLen - burst[i]._len,
				0);
		}
		pos += nextSize - headerLen;
	}
}

void NfbQueue::GetBuffers(size_t burstSize)
{
	while (ndp_tx_burst_get(_txQueue.get(), _txPacket.get(), burstSize) != burstSize)
		;
}

size_t NfbQueue::AlignBlockSize(size_t size)
{
	static constexpr size_t blockAlignment = 8;

	if (size % blockAlignment == 0) {
		return size;
	}

	const size_t offset = blockAlignment - (size % blockAlignment);
	return size + offset;
}

void NfbQueue::SendBurst(const PacketBuffer* burst)
{
	(void) burst;

	ndp_tx_burst_put(_txQueue.get());
	_isBufferInUse = false;
}

void NfbQueue::Flush()
{
	ndp_tx_burst_flush(_txQueue.get());
	_isBufferInUse = false;
}

size_t NfbQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

NfbPlugin::NfbPlugin(const std::string& params)
{
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

	DetermineSuperPacketSize();

	for (size_t id = 0; id < _queueCount; id++) {
		_queues.emplace_back(
			std::make_unique<NfbQueue>(_nfbDevice.get(), id, _burstSize, _superPacketSize));
	}
}

int NfbPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error(
			"Parameter \"queueCount\", \"burstSize\" or "
			"\"superPacketSize\" wrong format");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}

	if (_deviceName.empty()) {
		_logger->error("Required parameter \"device\" missing/empty");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

void NfbPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "device") {
			_deviceName = value;
		} else if (key == "queueCount") {
			_queueCount = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else if (key == "superPacket") {
			if (value == "no") {
				_superPackets = SuperPackets::Disable;
			} else if (value == "auto") {
				_superPackets = SuperPackets::Auto;
			} else if (value == "yes") {
				_superPackets = SuperPackets::Enable;
			} else {
				_logger->error("Unknown parameter value {}", value);
				throw std::runtime_error("NfbPlugin::ParseMap() has failed");
			}
		} else if (key == "superPacketSize") {
			_superPacketSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("NfbPlugin::ParseMap() has failed");
		}
	}
}

void NfbPlugin::DetermineSuperPacketSize()
{
	static const char* superCompName = "cesnet,ofm,frame_unpacker";

	int ret = nfb_comp_count(_nfbDevice.get(), superCompName);
	if (ret > 0) {
		if (_superPackets == SuperPackets::Auto) {
			_logger->info("SuperPackets enabled");
		} else if (_superPackets == SuperPackets::Disable) {
			_logger->warn("SuperPackets are supported, but disabled");
			_superPacketSize = 0;
		}
	} else if (ret == 0) {
		if (_superPackets == SuperPackets::Auto) {
			_logger->info("SuperPackets disabled/unsupported");
			_superPacketSize = 0;
		} else if (_superPackets == SuperPackets::Enable) {
			_logger->warn("SuperPackets are not supported, but enabled");
		} else if (_superPackets == SuperPackets::Disable) {
			_superPacketSize = 0;
		}
	} else {
		_logger->critical("nfb::nfb_comp_count() has failed, SuperPackets disabled");
		_superPacketSize = 0;
	}
}

size_t NfbPlugin::GetQueueCount() const noexcept
{
	return _queueCount;
}

OutputQueue* NfbPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

} // namespace replay
