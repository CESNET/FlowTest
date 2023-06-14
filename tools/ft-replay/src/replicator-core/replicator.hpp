/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../outputQueue.hpp"
#include "../packetQueueProvider.hpp"
#include "configParser.hpp"
#include "packetModifier.hpp"

#include "strategyFactory.hpp"

#include <functional>
#include <string>

namespace replay {

struct ReplicationUnit {
	/**
	 * @brief Construct a new Replication Unit object
	 *
	 * @param modifierStrategies
	 */
	ReplicationUnit(ModifierStrategies modifierStrategies)
		: packetModifier(std::move(modifierStrategies))
	{
	}

	/**
	 * @brief Strategies how to modify the packet
	 */
	PacketModifier packetModifier;
};

/**
 * @brief Replicate packets with replication units.
 */
class Replicator {
public:
	/**
	 * @brief Construct a new Replicator
	 *
	 * @param configParser Replication units configuration. It can be nullptr.
	 * @param outputQueue Output interface to send the packet
	 */
	Replicator(const ConfigParser* configParser, OutputQueue* outputQueue);

	/**
	 * @brief Replicates a given sequence of packets and sends them to the output interface
	 *
	 * Each given packet is replicated as many times as the number of replication units.
	 * Each replicated packet is modified by the packetModifier strategies
	 *
	 * @param begin Iterator to the begin of sequence
	 * @param end Iterator to the end of sequence
	 */
	void Replicate(PacketQueue::iterator begin, PacketQueue::iterator end);

	/**
	 * @brief Set the Replication Loop Id
	 *
	 * ID is required for loop packet modifier strategy
	 *
	 * @param replicationLoopId Current ID of replication loop
	 */
	void SetReplicationLoopId(uint64_t replicationLoopId) noexcept;

private:
	void UpdateAvailableReplicationUnits();

	size_t _maxBurstSize;
	uint64_t _replicationLoopId;

	std::unique_ptr<PacketBuffer[]> _packetBuffers;
	std::vector<ReplicationUnit> _replicationUnits;
	std::vector<ReplicationUnit*> _loopAvailableReplicationUnits;
	OutputQueue* _outputQueue;
};

} // namespace replay
