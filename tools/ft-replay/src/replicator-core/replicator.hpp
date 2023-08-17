/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../config.hpp"
#include "../outputQueue.hpp"
#include "../packetQueueProvider.hpp"
#include "../rateLimiter.hpp"
#include "configParser.hpp"
#include "packetModifier.hpp"
#include "strategyFactory.hpp"

#include <utility>

namespace replay {

struct ReplicationUnit {
	/**
	 * @brief Construct a new Replication Unit object
	 *
	 * @param modifierStrategies
	 */
	ReplicationUnit(ModifierStrategies modifierStrategies = {})
		: packetModifier(std::move(modifierStrategies))
	{
	}

	/**
	 * @brief Strategies how to modify the packet
	 */
	PacketModifier packetModifier;
};

/**
 * @class Replicator
 * @brief The Replicator class replicates packets from a source queue to an output queue based on
 * strategies and rate limiting configurations.
 */
class Replicator {
public:
	/**
	 * @brief Constructor for the Replicator class.
	 * @param packetQueue A unique pointer to the source PacketQueue.
	 * @param outputQueue A pointer to the OutputQueue where replicated packets will be sent.
	 * @param loopTimeDuration The duration of a single replication loop in nanoseconds.
	 */
	Replicator(
		std::unique_ptr<PacketQueue> packetQueue,
		OutputQueue* outputQueue,
		uint64_t loopTimeDuration);

	/**
	 * @brief Sets the rate limiter configuration for the Replicator.
	 * @param rateLimiterConfig The configuration for rate limiting.
	 */
	void SetRateLimiter(const Config::RateLimit& rateLimiterConfig);

	/**
	 * @brief Sets the replication strategy for the Replicator.
	 * @details The Replicator can be configured with multiple replication units, each having its
	 * own strategy. If a ConfigParser is provided, the Replicator sets its replication units
	 * according to the configurations. If no ConfigParser is provided, a default replication unit
	 * with a default strategy will be used.
	 * @param configParser A pointer to the ConfigParser object containing the replication strategy
	 * configurations.
	 */
	void SetReplicatorStrategy(const ConfigParser* configParser);

	/**
	 * @brief Initiates the replication process for a given replication loop ID.
	 * @param replicationLoopId The ID of the current replication loop.
	 * @details This method replicates packets from the source queue to the output queue based on
	 * the configured replication strategy and rate limiting settings.
	 */
	void Replicate(uint64_t replicationLoopId);

private:
	void SetAvailableReplicationUnits(uint64_t replicationLoopId);
	void SetDefaultReplicatorStrategy();
	void FillPacketBuffers(uint64_t replicatedPackets, size_t burstSize);
	uint64_t GetBurstSize(uint64_t replicatedPackets);
	uint64_t GetNumberOfPacketToReplicate() const noexcept;
	uint64_t GetMinimumRequiredTokens(uint64_t replicatedPackets);
	uint64_t GetIndexToPacketQueue(uint64_t replicatedPackets) const noexcept;
	uint64_t GetReplicationUnitIndex(uint64_t replicatedPackets) const noexcept;
	void WaitUntilEndOfTheLoop();
	void CopyPacketsToBuffer(uint64_t replicatedPackets, uint64_t burstSize);
	void ModifyPackets(uint64_t replicatedPackets, uint64_t burstSize, uint64_t replicationLoopId);

	struct BurstInfo {
		uint64_t burstSize;
		uint64_t usedTokens;
	};

	BurstInfo ConvertTokensToBurstSize(
		uint64_t replicatedPackets,
		uint64_t maximalPossibleBurstSize,
		uint64_t availableTokens);

	uint64_t _maxBurstSize;
	uint64_t _lastPacketTimestamp;
	uint64_t _loopTimeDuration;

	Config::RateLimit _rateLimiterConfig;
	RateLimiter _rateLimiter;

	std::unique_ptr<PacketBuffer[]> _packetBuffers;
	std::vector<ReplicationUnit> _replicationUnits;
	std::vector<ReplicationUnit*> _loopAvailableReplicationUnits;
	PacketQueue _packetQueue;
	OutputQueue* _outputQueue;
};

} // namespace replay
