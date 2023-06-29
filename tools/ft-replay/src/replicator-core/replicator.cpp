/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "replicator.hpp"

#include "configParser.hpp"
#include "configParserFactory.hpp"
#include "strategyFactory.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace replay {

Replicator::Replicator(const ConfigParser* configParser, OutputQueue* outputQueue)
	: _outputQueue(outputQueue)
{
	if (!configParser) {
		ReplicationUnit replicationUnit(ModifierStrategies {});
		_replicationUnits.emplace_back(std::move(replicationUnit));
	} else {
		StrategyFactory strategyFactory;
		const auto& loopStrategy = configParser->GetLoopStrategyDescription();

		for (const auto& unitStrategy : configParser->GetUnitsStrategyDescription()) {
			ReplicationUnit replicationUnit(strategyFactory.Create(unitStrategy, loopStrategy));
			_replicationUnits.emplace_back(std::move(replicationUnit));
		}
	}

	_loopAvailableReplicationUnits.reserve(_replicationUnits.size());
	SetReplicationLoopId(0);

	_maxBurstSize = outputQueue->GetMaxBurstSize();
	_packetBuffers = std::make_unique<PacketBuffer[]>(_maxBurstSize);
}

void Replicator::SetReplicationLoopId(uint64_t replicationLoopId) noexcept
{
	_replicationLoopId = replicationLoopId;
	UpdateAvailableReplicationUnits();
}

void Replicator::UpdateAvailableReplicationUnits()
{
	_loopAvailableReplicationUnits.clear();
	for (auto& replicationUnit : _replicationUnits) {
		if (replicationUnit.packetModifier.IsEnabledThisLoop(_replicationLoopId)) {
			_loopAvailableReplicationUnits.emplace_back(&replicationUnit);
		}
	}
}

void Replicator::Replicate(PacketQueue::iterator begin, PacketQueue::iterator end)
{
	size_t availReplicationUnits = _loopAvailableReplicationUnits.size();

	uint32_t maxPacketID = availReplicationUnits * std::distance(begin, end);
	uint32_t packetID = 0;

	/**
	 * Iterate over all replication packets {availReplicationUnits * (end - begin)
	 */
	while (packetID != maxPacketID) {
		size_t burstSize = std::min(maxPacketID - packetID, (uint32_t) _maxBurstSize);

		// fill buffers with requested length
		for (uint32_t idx = 0; idx < burstSize; idx++) {
			uint32_t packetQueueID = (idx + packetID) / availReplicationUnits;
			_packetBuffers[idx]._len = (*(begin + packetQueueID))->dataLen;
		}

		// get memory for request
		_outputQueue->GetBurst(_packetBuffers.get(), burstSize);

		// copy raw packet to output queue buffer
		for (size_t idx = 0; idx < burstSize; idx++) {
			uint32_t packetQueueID = (idx + packetID) / availReplicationUnits;
			std::byte* data = (*(begin + packetQueueID))->data.get();
			std::copy(data, data + _packetBuffers[idx]._len, _packetBuffers[idx]._data);
		}

		// modify packet with replication unit modifier strategies
		for (uint32_t idx = 0; idx < burstSize; idx++) {
			uint32_t replicationUnitID = (idx + packetID) % availReplicationUnits;
			uint32_t packetQueueID = (idx + packetID) / availReplicationUnits;
			_loopAvailableReplicationUnits[replicationUnitID]->packetModifier.Modify(
				_packetBuffers[idx]._data,
				(*(begin + packetQueueID))->info,
				_replicationLoopId);
		}

		// send packet to the output interface
		_outputQueue->SendBurst(_packetBuffers.get());

		packetID += burstSize;
	}
}

} // namespace replay
