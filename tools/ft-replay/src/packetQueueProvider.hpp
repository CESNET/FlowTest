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

private:
	uint8_t GetPacketQueueId(const Packet& packet);

	std::vector<std::unique_ptr<PacketQueue>> _packetQueues;
	size_t _queueCount;
};

} // namespace replay
