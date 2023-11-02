/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief NFB plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfb.hpp"

extern "C" {
#include <libfdt.h>
#include <nfb/ndp.h>
#include <nfb/nfb.h>
}

#include <cstddef>
#include <map>
#include <memory>

namespace replay {

void SuperPacketHeader::Clear() noexcept
{
	std::memset(this, 0, sizeof(*this));
}

void SuperPacketHeader::SetLength(uint16_t length) noexcept
{
	_length = htole16(length);
}

void SuperPacketHeader::SetHasNextHeader(bool value) noexcept
{
	_hasNextHeader = !!value;
}

NfbQueue::NfbQueue(
	nfb_device* dev,
	unsigned int queue_id,
	size_t burstSize,
	size_t superPacketSize,
	size_t superPacketLimit)
	: _txPacket(std::make_unique<ndp_packet[]>(burstSize))
	, _maxBurstSize(burstSize)
	, _superPacketSize(superPacketSize)
	, _superPacketLimit(superPacketLimit)
	, _lastBurstTotalPacketLen(0)
	, _lastBurstSize(0)
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
	if (burstSize > _maxBurstSize) {
		_logger->error(
			"Requested burstSize {} is bigger than predefined {}",
			burstSize,
			_maxBurstSize);
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}

	_lastBurstTotalPacketLen = 0;

	if (_superPacketSize) {
		GetSuperBurst(burst, burstSize);
	} else {
		GetRegularBurst(burst, burstSize);
	}

	_isBufferInUse = true;
}

void NfbQueue::GetRegularBurst(PacketBuffer* burst, size_t burstSize)
{
	static constexpr size_t MinPacketSize = 64;

	for (unsigned i = 0; i < burstSize; i++) {
		if (burst[i]._len < MinPacketSize) {
			_txPacket[i].data_length = MinPacketSize;
			_outputQueueStats.upscaledPackets++;
		} else {
			_txPacket[i].data_length = burst[i]._len;
		}

		_lastBurstTotalPacketLen += _txPacket[i].data_length;
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
	_lastBurstSize = burstSize;
}

void NfbQueue::GetSuperBurst(PacketBuffer* burst, size_t burstSize)
{
	static constexpr size_t HeaderLen = sizeof(SuperPacketHeader);
	static constexpr size_t MinPacketSize = 64;

	// fill _txPacket data_len
	unsigned txSuperPacketIndex = 0;
	size_t txSuperPacketCount = 0;
	_txPacket[0].data_length = 0;
	for (unsigned i = 0; i < burstSize; i++) {
		size_t nextSize = HeaderLen + std::max(burst[i]._len, MinPacketSize);
		nextSize = AlignBlockSize(nextSize);

		if (_txPacket[txSuperPacketIndex].data_length + nextSize <= _superPacketSize
			&& txSuperPacketCount < _superPacketLimit) {
			_txPacket[txSuperPacketIndex].data_length += nextSize;
			txSuperPacketCount++;
		} else {
			_txPacket[++txSuperPacketIndex].data_length = nextSize;
			txSuperPacketCount = 0;
		}
	}
	txSuperPacketIndex++;

	GetBuffers(txSuperPacketIndex);

	// assign buffers
	txSuperPacketIndex = 0;
	unsigned i, pos = 0;
	for (i = 0; i < burstSize; i++) {
		size_t pktLen = std::max(burst[i]._len, MinPacketSize);
		_lastBurstTotalPacketLen += pktLen;
		size_t nextSize = HeaderLen + pktLen;
		nextSize = AlignBlockSize(nextSize);
		// next packet/break condition
		if (pos + nextSize > _txPacket[txSuperPacketIndex].data_length) {
			txSuperPacketIndex++;
			pos = 0;
		}
		// fill header
		SuperPacketHeader* hdrPtr
			= reinterpret_cast<SuperPacketHeader*>(_txPacket[txSuperPacketIndex].data + pos);
		hdrPtr->Clear();
		hdrPtr->SetLength(pktLen);
		if (pos + nextSize >= _txPacket[txSuperPacketIndex].data_length) {
			hdrPtr->SetHasNextHeader(false);
		} else {
			hdrPtr->SetHasNextHeader(true);
		}

		pos += HeaderLen;

		// assign buffer
		burst[i]._data = reinterpret_cast<std::byte*>(_txPacket[txSuperPacketIndex].data + pos);
		// fill padding with zeros
		if (pktLen != burst[i]._len) {
			std::fill_n(
				_txPacket[txSuperPacketIndex].data + pos + burst[i]._len,
				pktLen - burst[i]._len,
				0);
		}
		pos += nextSize - HeaderLen;
	}
	_lastBurstSize = burstSize;
}

void NfbQueue::GetBuffers(size_t burstSize)
{
	while (ndp_tx_burst_get(_txQueue.get(), _txPacket.get(), burstSize) != burstSize)
		;
}

size_t NfbQueue::AlignBlockSize(size_t size)
{
	static constexpr size_t BlockAlignment = 8;

	if (size % BlockAlignment == 0) {
		return size;
	}

	const size_t offset = BlockAlignment - (size % BlockAlignment);
	return size + offset;
}

void NfbQueue::SendBurst(const PacketBuffer* burst)
{
	(void) burst;

	ndp_tx_burst_put(_txQueue.get());
	_isBufferInUse = false;

	_outputQueueStats.transmittedPackets += _lastBurstSize;
	_outputQueueStats.transmittedBytes += _lastBurstTotalPacketLen;
	_outputQueueStats.UpdateTime();
}

void NfbQueue::Flush()
{
	ndp_tx_burst_flush(_txQueue.get());
	_isBufferInUse = false;
}

size_t NfbQueue::GetMaxBurstSize() const noexcept
{
	return _maxBurstSize;
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

	size_t superPacketLimit = 0;
	if (_superPacketSize != 0) {
		superPacketLimit = GetSuperPacketLimit();
	}

	for (size_t id = 0; id < _queueCount; id++) {
		_queues.emplace_back(std::make_unique<NfbQueue>(
			_nfbDevice.get(),
			id,
			_burstSize,
			_superPacketSize,
			superPacketLimit));
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

size_t NfbPlugin::GetSuperPacketLimit()
{
	int len;
	uint32_t limit = 0;

	const void* fdt = nfb_get_fdt(_nfbDevice.get());
	if (fdt == nullptr) {
		_logger->error("Cannot load device tree");
		throw std::runtime_error("NfbPlugin::GetSuperPacketLimit() has failed");
	}

	int offset
		= fdt_path_offset(fdt, "/firmware/mi_bus0/application/replicator_core_0/frame_unpacker/");
	const void* buffer = fdt_getprop(fdt, offset, "unpack_limit", &len);

	if (len == sizeof(limit)) {
		limit = fdt32_to_cpu(*reinterpret_cast<const uint32_t*>(buffer));
	} else {
		_logger->error("Unexpected format of unpack_limit");
		throw std::runtime_error("NfbPlugin::GetSuperPacketLimit() has failed");
	}

	if (limit == 0) {
		_logger->error("Unpack_limit is zero");
		throw std::runtime_error("NfbPlugin::GetSuperPacketLimit() has failed");
	}

	return limit;
}

size_t NfbPlugin::GetQueueCount() const noexcept
{
	return _queueCount;
}

Offloads NfbPlugin::ConfigureOffloads(const OffloadRequests& offloads)
{
	(void) offloads;
	return 0;
}

OutputQueue* NfbPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

} // namespace replay
