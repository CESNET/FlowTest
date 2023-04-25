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
#include <numeric>

namespace replay {

PacketQueueProvider::PacketQueueProvider(size_t queueCount)
	: _queueCount(queueCount)
{
	_queuesDistribution.resize(_queueCount, {0, 0});
	for (size_t i = 0; i < _queueCount; i++) {
		_packetQueues.emplace_back(std::make_unique<PacketQueue>());
	}
}

void PacketQueueProvider::InsertPacket(std::unique_ptr<Packet> packet)
{
	uint8_t queueID = GetPacketQueueId(*packet);
	UpdateQueueDistribution(_queuesDistribution[queueID], packet->dataLen);
	_packetQueues[queueID]->emplace_back(std::move(packet));
}

void PacketQueueProvider::UpdateQueueDistribution(
	PacketQueueProvider::QueueDistribution& queueDistribution,
	size_t packetLength)
{
	queueDistribution.packets++;
	queueDistribution.bytes += packetLength;
}

uint8_t PacketQueueProvider::GetPacketQueueId(const Packet& packet)
{
	return packet.GetHash() % _queueCount;
}

PacketQueueProvider::QueueDistribution
PacketQueueProvider::GetPacketQueueRatioById(uint8_t queueId) const
{
	assert(queueId < _queueCount);

	auto queueDistributionSum = [](QueueDistribution sum, const QueueDistribution& distribution) {
		sum.packets += distribution.packets;
		sum.bytes += distribution.bytes;
		return sum;
	};

	auto queueDistributionTotal = std::accumulate(
		_queuesDistribution.begin(),
		_queuesDistribution.end(),
		QueueDistribution(),
		queueDistributionSum);

	QueueDistribution queueDistribution = _queuesDistribution[queueId];
	if (queueDistributionTotal.packets) {
		queueDistribution.packets /= queueDistributionTotal.packets;
	}
	if (queueDistributionTotal.bytes) {
		queueDistribution.bytes /= queueDistributionTotal.bytes;
	}

	return queueDistribution;
}

const std::unique_ptr<PacketQueue> PacketQueueProvider::GetPacketQueueById(uint8_t queueId)
{
	assert(queueId < _queueCount);
	return std::move(_packetQueues[queueId]);
}

} // namespace replay
