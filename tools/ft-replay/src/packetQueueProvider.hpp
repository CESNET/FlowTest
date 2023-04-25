/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Queue Provider interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "packet.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace replay {

using PacketQueue = std::vector<std::unique_ptr<Packet>>;

/**
 * @brief Provides packet queues
 *
 */
class PacketQueueProvider {
public:
	/**
	 * @brief Create packet queues
	 *
	 * @param queueCount Number of queues to create
	 */
	explicit PacketQueueProvider(size_t queueCount);

	/**
	 * @brief Insert packet into queue
	 *
	 * Queue ID is given by Packet hash
	 *
	 * @param packet Packet to insert
	 */
	void InsertPacket(std::unique_ptr<Packet> packet);

	/**
	 * @brief Get the ID specific queue
	 *
	 * @param queueId queue Id
	 * @return const std::unique_ptr<PacketQueue> Packet queue
	 */
	const std::unique_ptr<PacketQueue> GetPacketQueueById(uint8_t queueId);

	/**
	 * @brief Represents the percentage share of the total packets and bytes in a packet queue
	 *
	 * Calculated against the total number of packets and bytes in all queues.
	 */
	struct QueueDistribution {
		double packets; /**< The percentage share of the packets in the queue */
		double bytes; /**< The percentage share of the bytes in the queue */
	};

	/**
	 * @brief Retrieves the percentage share of the total packets and bytes in the specified queue
	 *
	 * This function returns a pair of doubles representing the percentage share of the total
	 * packets and bytes in the queue with the given queue ID, as calculated against the total
	 * number of packets and bytes in all queues.
	 *
	 * @param queueId The ID of the packet queue for which to calculate the percentage share.
	 * @return A pair of doubles representing the percentage share of packets and bytes in the
	 * specified queue.
	 */
	QueueDistribution GetPacketQueueRatioById(uint8_t queueId) const;

private:
	uint8_t GetPacketQueueId(const Packet& packet);
	void UpdateQueueDistribution(QueueDistribution& queueDistribution, size_t packetLength);

	std::vector<QueueDistribution> _queuesDistribution;
	std::vector<std::unique_ptr<PacketQueue>> _packetQueues;
	size_t _queueCount;
};

} // namespace replay
