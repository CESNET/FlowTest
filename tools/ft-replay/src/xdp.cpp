/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "xdp.hpp"

#include "ethTool.hpp"

#include <arpa/inet.h>
#include <linux/if_link.h>
#include <linux/if_packet.h>
#include <linux/sockios.h>
#include <memory>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace replay {

Umem::Umem(const QueueConfig& cfg)
	: _bufferSize(cfg._packetSize * cfg._umemSize)
{
	struct xsk_umem_config umemConfig {};

	umemConfig.fill_size = cfg._xskQueueSize;
	umemConfig.comp_size = cfg._xskQueueSize;
	umemConfig.frame_size = cfg._packetSize;
	umemConfig.frame_headroom = 0;
	umemConfig.flags = 0;

	_buffer.reset(reinterpret_cast<std::byte*>(AllocateBuffer(_bufferSize)));

	int ret = xsk_umem__create(
		&_umem,
		reinterpret_cast<void*>(_buffer.get()),
		_bufferSize,
		&_fq,
		&_cq,
		&umemConfig);
	if (ret) {
		_logger->error("Cannot register UMEM. Maybe insufficient privileges.");
		throw std::runtime_error("Umem::Umem() has failed");
	}
}

Umem::~Umem()
{
	xsk_umem__delete(_umem);
}

std::byte* Umem::operator[](uint64_t addr)
{
	if (addr > _bufferSize) {
		_logger->error("Index out of bounds");
		throw std::runtime_error("Umem::operator[] has failed");
	}
	return &_buffer[addr];
}

struct xsk_umem* Umem::GetXskUmem()
{
	return _umem;
}

struct xsk_ring_cons* Umem::GetCompQueue()
{
	return &_cq;
}

void* Umem::AllocateBuffer(size_t bufferSize)
{
	void* umemBuffer;

	int ret = posix_memalign(&umemBuffer, getpagesize(), bufferSize);
	if (ret != 0) {
		_logger->error("Cannot allocate buffer");
		throw std::runtime_error("Umem::AllocateBuffer() has failed");
	}
	return umemBuffer;
}

XdpSocket::XdpSocket(const QueueConfig& cfg, Umem& umem, size_t id)
{
	struct xsk_socket_config socketConfig {};

	socketConfig.rx_size = 0;
	socketConfig.tx_size = cfg._xskQueueSize;
	socketConfig.libbpf_flags = 0;
	socketConfig.xdp_flags = cfg._xdpFlags;
	socketConfig.bind_flags = cfg._bindFlags;

	int ret = xsk_socket__create(
		&_socket,
		cfg._ifcName.c_str(),
		id,
		umem.GetXskUmem(),
		nullptr,
		&_tx,
		&socketConfig);

	if (ret) {
		_logger->error("Cannot create socket");
		throw std::runtime_error("XdpSocket::XdpSocket() has failed");
	}

	_descriptor = xsk_socket__fd(_socket);
}

XdpSocket::~XdpSocket()
{
	xsk_socket__delete(_socket);
}

struct xsk_ring_prod* XdpSocket::GetTx()
{
	return &_tx;
}

int XdpSocket::GetDescriptor()
{
	return _descriptor;
}

XdpQueue::XdpQueue(const QueueConfig& cfg, size_t id)
	: _pktSize(cfg._packetSize)
	, _bufferSize(cfg._umemSize * cfg._packetSize)
	, _burstSize(cfg._burstSize)
	, _lastBurstTotalPacketLen(0)
	, _xskQueueSize(cfg._xskQueueSize)
	, _umem(cfg)
	, _socket(cfg, _umem, id)
{
	if (cfg._bindFlags == XDP_COPY && xsk_ring_prod__needs_wakeup(_socket.GetTx())) {
		if (id == 0) {
			_logger->debug("Interface needs wakeup");
		}
		_wakeUpFlag = true;
	}
}

XdpQueue::~XdpQueue()
{
	while (_outstandingTx != 0) {
		CollectSlots();
	}
}

void XdpQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	uint32_t txId = 0;
	_lastBurstTotalPacketLen = 0;

	CollectSlots();

	while (_outstandingTx + burstSize > _xskQueueSize
		   || _outstandingTx + burstSize > _bufferSize / _pktSize
		   || xsk_ring_prod__reserve(_socket.GetTx(), burstSize, &txId) != burstSize) {
		CollectSlots();
	}

	for (unsigned i = 0; i < burstSize; i++) {
		struct xdp_desc* txDesc = xsk_ring_prod__tx_desc(_socket.GetTx(), txId++);
		txDesc->addr = _umemAddr;
		txDesc->len = burst[i]._len;
		_lastBurstTotalPacketLen += burst[i]._len;

		burst[i]._data = _umem[_umemAddr];
		_umemAddr += _pktSize;

		if (_umemAddr >= _bufferSize) {
			_umemAddr = 0;
		}
	}
	_lastBurstSize = burstSize;
}

void XdpQueue::SendBurst(const PacketBuffer* burst)
{
	(void) burst;

	xsk_ring_prod__submit(_socket.GetTx(), _lastBurstSize);
	_outstandingTx += _lastBurstSize;

	if (_wakeUpFlag) {
		sendto(_socket.GetDescriptor(), nullptr, 0, MSG_DONTWAIT, nullptr, 0);
	}

	_outputQueueStats.transmittedPackets += _lastBurstSize;
	_outputQueueStats.transmittedBytes += _lastBurstTotalPacketLen;
	_outputQueueStats.UpdateTime();
}

size_t XdpQueue::GetMaxBurstSize() const noexcept
{
	return _burstSize;
}

void XdpQueue::CollectSlots()
{
	if (_outstandingTx == 0) {
		return;
	}

	if (_wakeUpFlag) {
		sendto(_socket.GetDescriptor(), nullptr, 0, MSG_DONTWAIT, nullptr, 0);
	}

	uint32_t compId;
	size_t completed = xsk_ring_cons__peek(_umem.GetCompQueue(), _outstandingTx, &compId);

	xsk_ring_cons__release(_umem.GetCompQueue(), completed);
	_outstandingTx -= completed;
}

XdpPlugin::XdpPlugin(const std::string& params)
{
	ParseArguments(params);

	EthTool ethTool(_cfg._ifcName);

	if (!ethTool.GetDeviceState()) {
		_logger->error("Selected interface is down");
		throw std::invalid_argument("XdpPlugin::XdpPlugin() has failed");
	}

	struct EthTool::Channels channels = ethTool.GetChannels();
	size_t devQueueCount = channels._txCount + channels._combinedCount;
	if (devQueueCount == 0) {
		_logger->error("No available TX channels");
		throw std::invalid_argument("XdpPlugin::XdpPlugin() has failed");
	}

	if (_cfg._queueCount == 0) {
		_cfg._queueCount = devQueueCount;
	} else if (devQueueCount < _cfg._queueCount) {
		_logger->error("Parameter queueCount is bigger than available channels count");
		throw std::invalid_argument("XdpPlugin::XdpPlugin() has failed");
	}

	struct EthTool::Driver driver = ethTool.GetDriver();
	size_t idOffset = 0;
	if (_cfg._bindFlags == XDP_ZEROCOPY && driver._driver.find("mlx") != std::string::npos) {
		_logger->info("Mellanox driver detected, added channels id offset");
		idOffset = devQueueCount;
	}

	PrintSettings();

	for (size_t id = 0; id < _cfg._queueCount; id++) {
		_queues.emplace_back(std::make_unique<XdpQueue>(_cfg, id + idOffset));
	}
}

size_t XdpPlugin::GetQueueCount() const noexcept
{
	return _cfg._queueCount;
}

OutputQueue* XdpPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

Offloads XdpPlugin::ConfigureOffloads(const OffloadRequests& offloads)
{
	(void) offloads;
	return 0;
}

void XdpPlugin::PrintSettings()
{
	std::string report = "Used settings: zeroCopy=";

	if (_cfg._bindFlags == XDP_ZEROCOPY) {
		report += "on";
	} else if (_cfg._bindFlags == XDP_COPY) {
		report += "off";
	} else {
		report += "unknown";
	}
	report += ", nativeMode=";
	if (_cfg._xdpFlags == XDP_FLAGS_DRV_MODE) {
		report += "on";
	} else if (_cfg._xdpFlags == XDP_FLAGS_SKB_MODE) {
		report += "off";
	} else {
		report += "unknown";
	}
	report += ", queueCount=" + std::to_string(_cfg._queueCount)
		+ ", packetSize=" + std::to_string(_cfg._packetSize) + ".";

	_logger->info(report);
}

bool XdpPlugin::PowerOfTwo(uint64_t value)
{
	return value && (!(value & (value - 1)));
}

bool XdpPlugin::CaseInsensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	const auto cmp = [](char lhsLetter, char rhsLetter) {
		return std::tolower(lhsLetter) == std::tolower(rhsLetter);
	};

	return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), cmp);
}

bool XdpPlugin::StrToBool(std::string_view str)
{
	for (const auto& item : {"yes", "true", "on", "1"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return true;
		}
	}

	for (const auto& item : {"no", "false", "off", "0"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return false;
		}
	}
	_logger->error("Unable to convert '{}' to boolean", std::string(str));
	throw std::invalid_argument("XdpPlugin::StrToBool() has failed");
}

void XdpPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "ifc") {
			_cfg._ifcName = value;
		} else if (key == "queueCount") {
			_cfg._queueCount = std::stoul(value);
		} else if (key == "packetSize") {
			_cfg._packetSize = std::stoul(value);
		} else if (key == "burstSize") {
			_cfg._burstSize = std::stoul(value);
		} else if (key == "umemSize") {
			_cfg._umemSize = std::stoul(value);
		} else if (key == "xskQueueSize") {
			_cfg._xskQueueSize = std::stoul(value);
		} else if (key == "zeroCopy") {
			if (StrToBool(value)) {
				_cfg._bindFlags = XDP_ZEROCOPY;
			} else {
				_cfg._bindFlags = XDP_COPY;
			}
		} else if (key == "nativeMode") {
			if (StrToBool(value)) {
				_cfg._xdpFlags = XDP_FLAGS_DRV_MODE;
			} else {
				_cfg._xdpFlags = XDP_FLAGS_SKB_MODE;
			}
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("XdpPlugin::ParseMap() has failed");
		}
	}
}

int XdpPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error("Wrong format of parameter, its not a number");
		throw std::invalid_argument("XdpPlugin::ParseArguments() has failed");
	}

	if (!PowerOfTwo(_cfg._packetSize) || !PowerOfTwo(_cfg._umemSize)
		|| !PowerOfTwo(_cfg._xskQueueSize)) {
		_logger->error(
			"Parameter \"packetSize\", \"bufferSize\" or \"xskQueueSize\" is not power of 2");
		throw std::invalid_argument("XdpPlugin::ParseArguments() has failed");
	}

	if (_cfg._xdpFlags == XDP_FLAGS_SKB_MODE && _cfg._bindFlags == XDP_ZEROCOPY) {
		_logger->error("ZeroCopy cannot be enabled with nativeMode off");
		throw std::invalid_argument("XdpPlugin::ParseArguments() has failed");
	}

	if (_cfg._ifcName.empty()) {
		_logger->error("Required parameter \"ifc\" missing/empty");
		throw std::invalid_argument("XdpPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
