/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Queue Provider interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "packet.hpp"
#include "timeDuration.hpp"

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

	/**
	 * @brief Get the time duration of packets in the PacketQueueProvider.
	 *
	 * This function returns the time duration of packets stored in the PacketQueueProvider.
	 *
	 * @note If no packets have been processed yet or if the internal TimeDuration object
	 * has not been updated with any timestamps, this function will return 0.
	 *
	 * @return The time duration of packets in nanoseconds.
	 */
	uint64_t GetPacketsTimeDuration() const noexcept;

	/**
	 * @brief Print statistics about the distribution of packets and bytes in each queue.
	 *
	 * This function prints statistics about the distribution of packets and bytes in each queue.
	 * For each queue, it calculates and logs the percentage share of packets and bytes in that
	 * queue compared to the total packets and bytes in all queues.
	 *
	 * The statistics are logged using the configured logger and include the queue ID, the
	 * percentage share of packets, and the percentage share of bytes.
	 */
	void PrintStats() const;

private:
	uint8_t GetPacketQueueId(const Packet& packet);
	void UpdateQueueDistribution(QueueDistribution& queueDistribution, size_t packetLength);

	std::vector<QueueDistribution> _queuesDistribution;
	std::vector<std::unique_ptr<PacketQueue>> _packetQueues;
	TimeDuration _timeDuration;

	size_t _queueCount;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PacketQueueProvider");
};

} // namespace replay
