/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Output plugin for AF_PACKET interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "afPacket.hpp"

#include "ethTool.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "protocol/ethernet.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace replay {

constexpr auto QUEUE_FLUSH_TIMEOUT = 3s;

OutputPluginFactoryRegistrator<AfPacketPlugin> afPacketPluginRegistration("packet");

/**
 * @brief Get only status flags related to frame transmission in a TX ring.
 *
 * This is workaround for kernel bug.
 * See kernel commit 171c3b151118a2fe0fc1e2a9d1b5a1570cfe82d2
 * @param[in] tp_status Frame status from its header
 * @return Frame transmission status
 */
static uint32_t FrameMaskStatus(uint32_t tp_status)
{
	return tp_status & ~(TP_STATUS_TS_SOFTWARE | TP_STATUS_TS_RAW_HARDWARE);
}

static bool IsFrameAvailable(uint32_t tp_status)
{
	return FrameMaskStatus(tp_status) == TP_STATUS_AVAILABLE;
}

static constexpr size_t GetMaxPacketSize(size_t frameSize) noexcept
{
	const size_t headerSize = TPACKET_ALIGN(sizeof(struct tpacket2_hdr));

	assert(frameSize >= headerSize);
	return frameSize - headerSize;
}

AfPacketQueue::AfPacketQueue(const AfPacketConfig& cfg, size_t queueId)
	: _config(cfg)
	, _queueId(queueId)
{
	SocketSetup();
	MemorySetup();
}

AfPacketQueue::~AfPacketQueue()
{
	auto start {std::chrono::steady_clock::now()};
	auto end = start;
	size_t outstandingTx = 0;

	// Wait for all packets to be sent
	for (size_t i = 0; i < _config._frameCount; ++i) {
		std::byte* framePtr = GetFrame(i);
		tpacket2_hdr* frameHdr = reinterpret_cast<tpacket2_hdr*>(framePtr);

		while (!IsFrameAvailable(frameHdr->tp_status) && end - start < QUEUE_FLUSH_TIMEOUT) {
			(void) sendto(_socket.GetSocketId(), nullptr, 0, MSG_DONTWAIT, nullptr, 0);
			end = std::chrono::steady_clock::now();
		}

		if (!IsFrameAvailable(frameHdr->tp_status)) {
			outstandingTx++;
		}
	}

	if (outstandingTx > 0) {
		_logger->warn("Queue (ID {}) did not return all remaining packet buffers.", _queueId);
	}
}

void AfPacketQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	assert(_lastBurstSize == 0 && "GetBurst() called before the previous request was processed");

	_lastBurstTotalPacketLen = 0;

	for (size_t i = 0; i < burstSize; i++) {
		const size_t frameIndex = (_ringIndex + i) % _config._frameCount;
		std::byte* framePtr = GetFrame(frameIndex);
		tpacket2_hdr* frameHdr = reinterpret_cast<tpacket2_hdr*>(framePtr);
		std::byte* framePayload = framePtr + TPACKET_ALIGN(sizeof(struct tpacket2_hdr));

		while (!IsFrameAvailable(frameHdr->tp_status)) {
			// Wait for the frame to be available
			TxPoll();
		}

		PacketBuffer& buffer = burst[i];
		assert(buffer._len <= GetMaxPacketSize(_config._frameSize));

		frameHdr->tp_len = buffer._len;
		buffer._data = framePayload;
		_lastBurstTotalPacketLen += buffer._len;
	}

	_lastBurstSize = burstSize;
}

void AfPacketQueue::SendBurst(const PacketBuffer* burst)
{
	(void) burst;

	// Mark all packets as ready to be send
	for (size_t i = 0; i < _lastBurstSize; i++) {
		const size_t frameIndex = (_ringIndex + i) % _config._frameCount;
		std::byte* framePtr = GetFrame(frameIndex);
		tpacket2_hdr* frameHdr = reinterpret_cast<tpacket2_hdr*>(framePtr);

		frameHdr->tp_status = TP_STATUS_SEND_REQUEST;
	}

	// Kick-off transmits
	while (sendto(_socket.GetSocketId(), nullptr, 0, MSG_DONTWAIT, nullptr, 0) < 0) {
		if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
			// Repeat until all the packets are queued
			continue;
		}

		/*
		 * It might fail here if one of the packet in the burst is considered
		 * "malformed". The packet should be marked with TP_STATUS_WRONG_FORMAT
		 * flag in its header status. When PACKET_LOSS option is set on the socket,
		 * malformed packets are ignored and frame is made available.
		 */
		_logger->error("Packet transmission has failed: {}", errno);
		throw std::runtime_error("AfPacketQueue::SendBurst() has failed");
	}

	_outputQueueStats.transmittedPackets += _lastBurstSize;
	_outputQueueStats.transmittedBytes += _lastBurstTotalPacketLen;
	_outputQueueStats.UpdateTime();

	_ringIndex = (_ringIndex + _lastBurstSize) % _config._frameCount;
	_lastBurstSize = 0;
	_lastBurstTotalPacketLen = 0;
}

size_t AfPacketQueue::GetMaxBurstSize() const noexcept
{
	return _config._burstSize;
}

void AfPacketQueue::SocketSetup()
{
	_socket.OpenSocket(PF_PACKET, SOCK_RAW, 0); // Raw packets, 0 == TX only

	SocketSetupPacketVersion();
	SocketSetupQdiskBypass();
	SocketSetupPacketLoss();
	SocketSetupTxRing();
	SocketSetupBinding();
}

void AfPacketQueue::SocketSetupPacketVersion()
{
	const int version = TPACKET_V2;
	int ret;

	ret = setsockopt(_socket.GetSocketId(), SOL_PACKET, PACKET_VERSION, &version, sizeof(version));
	if (ret < 0) {
		_logger->error("Failed to set PACKET_VERSION of AF_PACKET");
		throw std::runtime_error("AfPacketQueue::SocketSetupPacketVersion() has failed");
	}
}

void AfPacketQueue::SocketSetupQdiskBypass()
{
	const int en = _config._qdiskBypass ? 1 : 0;
	int ret;

	ret = setsockopt(_socket.GetSocketId(), SOL_PACKET, PACKET_QDISC_BYPASS, &en, sizeof(en));
	if (ret < 0) {
		_logger->error("Failed to set PACKET_QDISC_BYPASS of AF_PACKET");
		throw std::runtime_error("AfPacketQueue::SocketSetupQdiskBypass() has failed");
	}
}

void AfPacketQueue::SocketSetupPacketLoss()
{
	const int en = _config._packetLoss ? 1 : 0;
	int ret;

	ret = setsockopt(_socket.GetSocketId(), SOL_PACKET, PACKET_LOSS, &en, sizeof(en));
	if (ret < 0) {
		_logger->error("Failed to set PACKET_LOSS of AF_PACKET");
		throw std::runtime_error("AfPacketQueue::SocketSetupPacketLoss() has failed");
	}
}

void AfPacketQueue::SocketSetupTxRing()
{
	int ret;
	struct tpacket_req req;

	req.tp_block_size = _config._blockSize;
	req.tp_block_nr = _config._frameCount / (_config._blockSize / _config._frameSize);
	req.tp_frame_size = _config._frameSize;
	req.tp_frame_nr = _config._frameCount;

	assert(req.tp_frame_size * req.tp_frame_nr == req.tp_block_nr * req.tp_block_size);

	ret = setsockopt(_socket.GetSocketId(), SOL_PACKET, PACKET_TX_RING, &req, sizeof(req));
	if (ret < 0) {
		_logger->error("Failed to set PACKET_TX_RING of AF_PACKET");
		throw std::runtime_error("AfPacketQueue::SocketSetupTxRing() has failed");
	}
}

void AfPacketQueue::SocketSetupBinding()
{
	struct sockaddr_ll ll;
	int ret;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_protocol = 0; // We don't want to receive anything i.e. TX only
	ll.sll_ifindex = GetInterfaceIndex(_socket.GetSocketId(), _config._ifcName);

	ret = bind(_socket.GetSocketId(), reinterpret_cast<struct sockaddr*>(&ll), sizeof(ll));
	if (ret < 0) {
		_logger->error("Failed to bind the interface");
		throw std::runtime_error("AfPacketQueue::SocketSetupBinding() has failed");
	}
}

void AfPacketQueue::MemorySetup()
{
	const size_t mapSize = _config._frameCount * _config._frameSize;
	const int protection = PROT_READ | PROT_WRITE;
	const int flags = MAP_SHARED | MAP_LOCKED;

	_mapping = MemoryMapping(nullptr, mapSize, protection, flags, _socket.GetSocketId(), 0);
}

int AfPacketQueue::GetInterfaceIndex(int fd, std::string_view interface)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));

	if (interface.length() >= sizeof(ifr.ifr_name)) {
		_logger->error("Interface name is too long ({})", interface);
		goto error;
	}

	memcpy(ifr.ifr_name, interface.data(), interface.length());
	ifr.ifr_name[interface.length()] = '\0';

	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		_logger->error("Failed to retrieve interface index of {}", interface);
		goto error;
	}

	return ifr.ifr_ifindex;

error:
	throw std::invalid_argument("AfPacketQueue::GetInterfaceIndex() has failed");
}

std::byte* AfPacketQueue::GetFrame(size_t index)
{
	assert(index < _config._frameCount);

	void* memory = _mapping.GetData();
	return reinterpret_cast<std::byte*>(memory) + (index * _config._frameSize);
}

void AfPacketQueue::TxPoll()
{
	struct pollfd pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = _socket.GetSocketId();
	pfd.events = POLLOUT;
	pfd.revents = 0;

	if (poll(&pfd, 1, -1) < 0 || (pfd.revents & POLLERR) != 0) {
		_logger->error("poll() on TX ring has failed");
		throw std::runtime_error("AfPacketQueue::TxPoll");
	}
}

AfPacketPlugin::AfPacketPlugin(const std::string& params)
{
	ParseArguments(params);
	DetermineMaxPacketSize();

	EthTool ethTool(_config._ifcName);
	if (!ethTool.GetDeviceState()) {
		_logger->error("Selected interface is down");
		throw std::invalid_argument("AfPacketPlugin::AfPacketPlugin() has failed");
	}

	struct EthTool::Channels channels = ethTool.GetChannels();
	size_t devQueueCount = channels._txCount + channels._combinedCount;
	if (devQueueCount == 0) {
		_logger->error("No available TX channels");
		throw std::invalid_argument("AfPacketPlugin::AfPacketPlugin() has failed");
	}

	if (_config._queueCount == 0) {
		_config._queueCount = devQueueCount;
	} else if (_config._queueCount > devQueueCount) {
		// It works but there are no performance benefits
		_logger->warn("\"queueCount\" is greater than number of available TX channels");
	}

	PrintSettings();

	for (size_t id = 0; id < _config._queueCount; id++) {
		_queues.emplace_back(std::make_unique<AfPacketQueue>(_config, id));
	}
}

size_t AfPacketPlugin::GetQueueCount() const noexcept
{
	return _queues.size();
}

OutputQueue* AfPacketPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

size_t AfPacketPlugin::GetMTU() const noexcept
{
	return _maxPacketSize;
}

NumaNode AfPacketPlugin::GetNumaNode()
{
	return utils::GetInterfaceNumaNode(_config._ifcName);
}

void AfPacketPlugin::PrintSettings()
{
	std::string report = "Used setings:";

	report += " ifc=" + _config._ifcName;
	report += ", queueCount=" + std::to_string(_config._queueCount);
	report += ", burstSize=" + std::to_string(_config._burstSize);
	report += ", blockSize=" + std::to_string(_config._blockSize);
	report += ", packetSize=" + std::to_string(_config._frameSize);
	report += ", frameCount=" + std::to_string(_config._frameCount);
	report += ", qdiskBypass=" + std::string(_config._qdiskBypass ? "True" : "False");
	report += ", packetLoss=" + std::string(_config._packetLoss ? "True" : "False");

	_logger->info(report);
}

void AfPacketPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "ifc") {
			_config._ifcName = value;
		} else if (key == "queueCount") {
			_config._queueCount = std::stoul(value);
		} else if (key == "burstSize") {
			_config._burstSize = std::stoul(value);
		} else if (key == "blockSize") {
			_config._blockSize = std::stoul(value);
		} else if (key == "packetSize") {
			_config._frameSize = std::stoul(value);
		} else if (key == "frameCount") {
			_config._frameCount = std::stoul(value);
		} else if (key == "qdiskBypass") {
			_config._qdiskBypass = utils::StrToBool(value);
		} else if (key == "packetLoss") {
			_config._packetLoss = utils::StrToBool(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("AfPacketPlugin::ParseMap() has failed");
		}
	}
}

int AfPacketPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);
	size_t framesPerBlock;

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ex) {
		_logger->error("Wrong format of a parameter, it is not a valid number");
		goto error;
	}

	if (_config._ifcName.empty()) {
		_logger->error("Required parameter \"ifc\" missing/empty");
		goto error;
	}

	if (_config._burstSize == 0) {
		_logger->error("\"burstSize\" must be non-zero number");
		goto error;
	}

	if (_config._burstSize > _config._frameCount) {
		_logger->error("\"burstSize\" must be less than \"frameCount\"");
		goto error;
	}

	if (_config._blockSize == 0 || _config._blockSize % getpagesize() != 0) {
		const int pgSize = getpagesize();
		_logger->error("\"blockSize\" must be multiple of PAGE_SIZE ({})", pgSize);
		goto error;
	}

	if (!utils::PowerOfTwo(_config._blockSize)) {
		_logger->error("\"blockSize\" must be power of two");
		goto error;
	}

	if (!utils::PowerOfTwo(_config._frameSize)) {
		_logger->error("\"packetSize\" must be power of two");
		goto error;
	}

	if (_config._frameSize < TPACKET2_HDRLEN) {
		_logger->error("\"packetSize\" must be greater than {} bytes", TPACKET2_HDRLEN);
		goto error;
	}

	if (_config._frameSize % TPACKET_ALIGNMENT != 0) {
		_logger->error(
			"\"packetSize\" must be multiple of TPACKET_ALIGNMENT ({})",
			TPACKET_ALIGNMENT);
		goto error;
	}

	if (_config._frameSize > _config._blockSize) {
		_logger->error("\"packetSize\" must be less or equal to \"blockSize\"");
		goto error;
	}

	if (_config._blockSize % _config._frameSize != 0) {
		_logger->error("\"blockSize\" must be divisible by \"packetSize\"");
		goto error;
	}

	framesPerBlock = _config._blockSize / _config._frameSize;
	if (_config._frameCount % framesPerBlock != 0) {
		_logger->error("\"frameCount\" must be divisible by {}", framesPerBlock);
		goto error;
	}

	return argMap.size();

error:
	throw std::invalid_argument("AfPacketPlugin::ParseArguments() has failed");
}

void AfPacketPlugin::DetermineMaxPacketSize()
{
	const size_t ifcMaxSize
		= protocol::Ethernet::HEADER_SIZE + utils::GetInterfaceMTU(_config._ifcName);
	const size_t bufMaxSize = GetMaxPacketSize(_config._frameSize);

	if (ifcMaxSize <= bufMaxSize) {
		_logger->info(
			"Maximum packet size limited by interface MTU (+ Ethernet header size) ({} bytes)",
			ifcMaxSize);
	} else {
		_logger->info("Maximum packet size limited by packetSize parameter ({} bytes)", bufMaxSize);
	}

	_maxPacketSize = std::min(ifcMaxSize, bufMaxSize);
}

} // namespace replay
