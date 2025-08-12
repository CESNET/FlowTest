/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief dpdk queue interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputQueue.hpp"
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf_core.h>
#include <rte_mempool.h>

/**
 * @brief Structure for storing parameters
 *
 * Helper structure that is used to pass parameters
 * to dpdkQueue from dpdkPlugin.
 */
struct DpdkQueueParams {
	std::vector<uint16_t> ports; ///< IDs of DPDK output ports (supports at least one, at most two)
	uint16_t queueId; ///< Identification of a specific TX queue
	size_t MTUSize; ///< Maximal size of a packet to be send
	size_t poolSize; ///< Size of the memory pool (i.e. number of packets)
	size_t burstSize; ///< Maximum number of packets that can be send at one time
};

/**
 * @brief Structure for storing an mbuf with its length
 *
 * This structure is used as a wrapper for an mbuf. It
 * contains the mbuf as well as how many packets it contains.
 */
struct DpdkQueueMbufs {
	std::unique_ptr<struct rte_mbuf*[]> mbuf; ///< Array of packets to send
	size_t pktsToSend; ///< Number of packets stored in the mbuf
};

namespace replay {

/**
 * @brief DPDK output queue
 *
 * dpdkQueue allocates memory and sends packets through
 * one or two interfaces (ports) based on the configuration
 * recieved from dpdkPlugin. If the queue is sending through two
 * interfaces, it is able to divide packets between the interfaces
 * based on their IP addresses (ie. it guarantees that each direction
 * of the same biflow will go through a different interface).
 * Queues get created in the dpdkPlugin constructor.
 */
class DpdkQueue : public OutputQueue {
public:
	/**
	 * Constructor
	 *
	 * @param[in] params additional info recieved from dpdkPlugin used to initialize queues
	 */
	DpdkQueue(DpdkQueueParams& params);

	/**
	 * @brief Get buffers of desired size for packets
	 *
	 * Assigns buffers of size _len to _data
	 *
	 * @param[in,out] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of PacketBuffers
	 */
	void GetBurst(PacketBuffer* burst, size_t burstSize) override;

	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] pointer to PacketBuffer array
	 */
	void SendBurst(const PacketBuffer* burst) override;

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return size_t maximal possible burst size
	 */
	size_t GetMaxBurstSize() const noexcept override;

private:
	void DoBurst(uint16_t port, size_t pktsToSend, struct rte_mbuf** mbuf);
	int DetermineSocket();

	size_t _burstSize;
	size_t _poolSize;
	size_t _MTUSize;
	uint16_t _queueId;
	std::vector<uint16_t> _ports;

	std::unique_ptr<rte_mempool, decltype(&rte_mempool_free)> _pool {nullptr, &rte_mempool_free};
	std::unique_ptr<struct rte_mbuf*[]> _mainMbuf;
	std::vector<DpdkQueueMbufs> _mbufs;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("DpdkQueue");
};

} // namespace replay
