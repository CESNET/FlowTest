/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "outputQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <linux/if_link.h>
#include <linux/if_packet.h>
#include <memory>
#include <string>

#ifdef FT_XDP_LIBXDP
#include <xdp/xsk.h>
#else
#include <bpf/xsk.h>
#endif

namespace replay {

struct QueueConfig {
	std::string _ifcName;
	size_t _burstSize = 64;
	size_t _umemSize = 4096;
	size_t _xskQueueSize = 2048;
	size_t _packetSize = 2048;
	size_t _queueCount = 0;
	size_t _bindFlags = XDP_ZEROCOPY;
	size_t _xdpFlags = XDP_FLAGS_DRV_MODE;
};

class Umem {
public:
	/**
	 * @brief Umem constructor
	 *
	 * @param[in] Queue configuration
	 */
	explicit Umem(const QueueConfig& cfg);

	/**
	 * @brief Socket destructor
	 */
	~Umem();

	Umem(const Umem&) = delete;
	Umem(Umem&&) = delete;
	Umem& operator=(const Umem&) = delete;
	Umem& operator=(Umem&&) = delete;

	/**
	 * @brief Access Umem slot
	 *
	 * @param[in] Address of slot
	 *
	 * @return Pointer to umem slot
	 */
	std::byte* operator[](uint64_t addr);

	/**
	 * @brief Get pointer to xsk_umem
	 *
	 * @return Pointer to xsk_umem
	 */
	struct xsk_umem* GetXskUmem();

	/**
	 * @brief Get pointer completition queue
	 *
	 * @return Pointer to completition queue
	 */
	struct xsk_ring_cons* GetCompQueue();

private:
	void* AllocateBuffer(size_t bufferSize);

	size_t _bufferSize;
	struct xsk_umem* _umem;
	struct xsk_ring_prod _fq;
	struct xsk_ring_cons _cq;

	std::unique_ptr<std::byte[], decltype(&std::free)> _buffer {nullptr, &std::free};

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("Umem");
};

class XdpSocket {
public:
	/**
	 * @brief Socket constructor
	 *
	 * @param[in] Queue configuration
	 * @param[in] Umem
	 * @param[in] Queue id
	 */
	XdpSocket(const QueueConfig& cfg, Umem& umem, size_t id);

	/**
	 * @brief Socket destructor
	 */
	~XdpSocket();

	XdpSocket(const XdpSocket&) = delete;
	XdpSocket(XdpSocket&&) = delete;
	XdpSocket& operator=(const XdpSocket&) = delete;
	XdpSocket& operator=(XdpSocket&&) = delete;

	/**
	 * @brief Get pointer to TX queue
	 *
	 * @return TX queue pointer
	 */
	struct xsk_ring_prod* GetTx();

	/**
	 * @brief Get socket descriptor
	 *
	 * @return socket descriptor
	 */
	int GetDescriptor();

private:
	struct xsk_ring_prod _tx;
	struct xsk_socket* _socket;
	int _descriptor;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("XdpSocket");
};

class XdpQueue : public OutputQueue {
public:
	/**
	 * @brief Construct XdpQueue
	 *
	 * Constructs XdpQueue. XdpQueue is used to send packets via XDP.
	 *
	 * @param[in] Queue configuration
	 * @param[in] Queue id
	 */
	XdpQueue(const QueueConfig& cfg, size_t id);
	/**
	 * @brief XdpQueue destructor
	 */
	~XdpQueue();

	XdpQueue(const XdpQueue&) = delete;
	XdpQueue(XdpQueue&&) = delete;
	XdpQueue& operator=(const XdpQueue&) = delete;
	XdpQueue& operator=(XdpQueue&&) = delete;

	/**
	 * @brief Get buffers of desired size for packets
	 *
	 * Assigns buffers of size _len to _data
	 *
	 * @param[in,out] pointer to PacketBuffer array
	 * @param[in] number of PacketBuffers
	 *
	 * @return number of assigned buffers
	 */
	void GetBurst(PacketBuffer* burst, size_t burstSize) override;

	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] pointer to PacketBuffer array
	 * @param[in] number of packets to send
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	void CollectSlots();

	size_t _pktSize;
	size_t _bufferSize;
	size_t _burstSize;
	size_t _lastBurstTotalPacketLen;
	size_t _xskQueueSize;
	size_t _lastBurstSize = 0;

	bool _wakeUpFlag = false;
	uint32_t _outstandingTx = 0;
	uint64_t _umemAddr = 0;

	Umem _umem;
	XdpSocket _socket;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("XdpQueue");
};

class XdpPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct XdpPlugin
	 *
	 * Constructs XdpPlugin and XdpQueue.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit XdpPlugin(const std::string& params);

	/**
	 * @brief Default destructor
	 */
	~XdpPlugin() = default;

	XdpPlugin(const XdpPlugin&) = delete;
	XdpPlugin(XdpPlugin&&) = delete;
	XdpPlugin& operator=(const XdpPlugin&) = delete;
	XdpPlugin& operator=(XdpPlugin&&) = delete;

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

	/**
	 * @brief Determines and configure the available offloads.
	 *
	 * @param offloads The requested offloads to configure.
	 * @return Configured offloads.
	 */
	Offloads ConfigureOffloads(const OffloadRequests& offloads) override;

private:
	void PrintSettings();

	bool PowerOfTwo(uint64_t value);

	bool CaseInsensitiveCompare(std::string_view lhs, std::string_view rhs);

	bool StrToBool(std::string_view str);

	void ParseMap(const std::map<std::string, std::string>& argMap);

	int ParseArguments(const std::string& args);

	std::vector<std::unique_ptr<XdpQueue>> _queues;
	QueueConfig _cfg;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("XdpPlugin");
};

OutputPluginFactoryRegistrator<XdpPlugin> xdpPluginRegistration("xdp");

} // namespace replay
