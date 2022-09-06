/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Queue Provider implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetQueueProvider.hpp"

#include <cassert>
#include <memory>

namespace replay {

PacketQueueProvider::PacketQueueProvider(size_t queueCount)
	: _queueCount(queueCount)
{
	for (size_t i = 0; i < _queueCount; i++) {
		_packetQueues.emplace_back(std::make_unique<PacketQueue>());
	}
}

void PacketQueueProvider::InsertPacket(std::unique_ptr<Packet> packet)
{
	uint8_t queueID = GetPacketQueueId(*packet);
	_packetQueues[queueID]->emplace_back(std::move(packet));
}

uint8_t PacketQueueProvider::GetPacketQueueId(const Packet& packet)
{
	return packet.GetHash() % _queueCount;
}

const std::unique_ptr<PacketQueue> PacketQueueProvider::GetPacketQueueById(uint8_t queueId)
{
	assert(queueId < _queueCount);
	return std::move(_packetQueues[queueId]);
}

} // namespace replay
