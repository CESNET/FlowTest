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

namespace replay {

Replicator::Replicator(
	std::unique_ptr<PacketQueue> packetQueue,
	OutputQueue* outputQueue,
	uint64_t loopTimeDuration)
	: _packetQueue(std::move(*(packetQueue.get())))
	, _outputQueue(outputQueue)
{
	SetDefaultReplicatorStrategy();

	_loopTimeDuration = loopTimeDuration;
	_lastPacketTimestamp = 0;
	_maxBurstSize = outputQueue->GetMaxBurstSize();
	_packetBuffers = std::make_unique<PacketBuffer[]>(_maxBurstSize);
}

void Replicator::SetDefaultReplicatorStrategy()
{
	ModifierStrategies defaultStrategy = {};
	ReplicationUnit defaultReplicationUnit(std::move(defaultStrategy));
	_replicationUnits.clear();
	_replicationUnits.emplace_back(std::move(defaultReplicationUnit));
	SetPacketModifierChecksumOffloads();
}

void Replicator::SetReplicatorStrategy(const ConfigParser* configParser)
{
	if (!configParser) {
		SetDefaultReplicatorStrategy();
		return;
	}

	StrategyFactory strategyFactory;
	const auto& loopStrategy = configParser->GetLoopStrategyDescription();

	_replicationUnits.clear();
	_loopAvailableReplicationUnits.clear();

	for (const auto& unitStrategy : configParser->GetUnitsStrategyDescription()) {
		ReplicationUnit replicationUnit(strategyFactory.Create(unitStrategy, loopStrategy));
		_replicationUnits.emplace_back(std::move(replicationUnit));
	}
	SetPacketModifierChecksumOffloads();
}

void Replicator::SetRequestedOffloads(const OffloadRequests& reguestedOffloads)
{
	_reguestedOffloads = reguestedOffloads;

	SetPacketModifierChecksumOffloads();
	SetRateLimiter(reguestedOffloads.rateLimit);
}

void Replicator::SetPacketModifierChecksumOffloads()
{
	for (auto& replicationUnit : _replicationUnits) {
		replicationUnit.packetModifier.SetChecksumOffloads(_reguestedOffloads.checksumOffloads);
	}
}

void Replicator::SetRateLimiter(const Config::RateLimit& rateLimiterConfig)
{
	if (std::holds_alternative<std::monostate>(rateLimiterConfig)) {
		_rateLimiter.SetLimit(0);
	} else if (std::holds_alternative<Config::RateLimitPps>(rateLimiterConfig)) {
		_rateLimiter.SetLimit(std::get<Config::RateLimitPps>(rateLimiterConfig).value);
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimiterConfig)) {
		_rateLimiter.SetLimit(
			std::get<Config::RateLimitMbps>(rateLimiterConfig).ConvertToBytesPerSecond());
	} else if (std::holds_alternative<Config::RateLimitTimeUnit>(rateLimiterConfig)) {
		_rateLimiter.SetLimit(std::get<Config::RateLimitTimeUnit>(rateLimiterConfig).value);
	} else {
		throw std::logic_error("RateLimit config type is not implemented");
	}

	_rateLimiterConfig = rateLimiterConfig;
}

void Replicator::Replicate(uint64_t replicationLoopId)
{
	SetAvailableReplicationUnits(replicationLoopId);

	size_t packetsToReplicate = GetNumberOfPacketToReplicate();
	size_t replicatedPackets = 0;
	_lastPacketTimestamp = 0;

	while (replicatedPackets != packetsToReplicate) {
		uint64_t burstSize = GetBurstSize(replicatedPackets);

		FillPacketBuffers(replicatedPackets, burstSize);
		_outputQueue->GetBurst(_packetBuffers.get(), burstSize);
		CopyPacketsToBuffer(replicatedPackets, burstSize);
		ModifyPackets(replicatedPackets, burstSize, replicationLoopId);
		_outputQueue->SendBurst(_packetBuffers.get());

		replicatedPackets += burstSize;
	}

	WaitUntilEndOfTheLoop();
}

void Replicator::CopyPacketsToBuffer(uint64_t replicatedPackets, uint64_t burstSize)
{
	for (uint64_t idx = 0; idx < burstSize; idx++) {
		uint64_t packetQueueIndex = GetIndexToPacketQueue(idx + replicatedPackets);
		std::byte* data = _packetQueue[packetQueueIndex]->data.get();
		std::copy(data, data + _packetBuffers[idx]._len, _packetBuffers[idx]._data);
	}
}

void Replicator::ModifyPackets(
	uint64_t replicatedPackets,
	uint64_t burstSize,
	uint64_t replicationLoopId)
{
	for (uint64_t idx = 0; idx < burstSize; idx++) {
		uint64_t packetQueueIndex = GetIndexToPacketQueue(idx + replicatedPackets);
		uint64_t replicationUnitIndex = GetReplicationUnitIndex(idx + replicatedPackets);
		_loopAvailableReplicationUnits[replicationUnitIndex]->packetModifier.Modify(
			_packetBuffers[idx]._data,
			_packetQueue[packetQueueIndex]->info,
			replicationLoopId);
	}
}

void Replicator::SetAvailableReplicationUnits(uint64_t replicationLoopId)
{
	_loopAvailableReplicationUnits.clear();
	for (auto& replicationUnit : _replicationUnits) {
		if (replicationUnit.packetModifier.IsEnabledThisLoop(replicationLoopId)) {
			_loopAvailableReplicationUnits.emplace_back(&replicationUnit);
		}
	}
}

void Replicator::FillPacketBuffers(uint64_t replicatedPackets, uint64_t burstSize)
{
	for (uint64_t idx = 0; idx < burstSize; idx++) {
		uint64_t packetQueueIndex = GetIndexToPacketQueue(idx + replicatedPackets);
		_packetBuffers[idx]._len = _packetQueue[packetQueueIndex]->dataLen;
		_packetBuffers[idx]._info = &_packetQueue[packetQueueIndex]->info;
	}
}

void Replicator::WaitUntilEndOfTheLoop()
{
	if (!std::holds_alternative<Config::RateLimitTimeUnit>(_rateLimiterConfig)) {
		return;
	}

	uint64_t timeUntilEndOfLoop = _loopTimeDuration - _lastPacketTimestamp;
	_rateLimiter.GetAvailableTokens(timeUntilEndOfLoop);
	_rateLimiter.SetProcessedTokens(timeUntilEndOfLoop);
}

uint64_t Replicator::GetNumberOfPacketToReplicate() const noexcept
{
	return _packetQueue.size() * _loopAvailableReplicationUnits.size();
}

uint64_t Replicator::GetBurstSize(uint64_t replicatedPackets)
{
	uint64_t minimunRequiredTokens = GetMinimumRequiredTokens(replicatedPackets);
	uint64_t availableTokens = _rateLimiter.GetAvailableTokens(minimunRequiredTokens);
	uint64_t maxNuberOfPacketsToReplicate = GetNumberOfPacketToReplicate();
	uint64_t maxPossibleBurstSize
		= std::min(maxNuberOfPacketsToReplicate - replicatedPackets, _maxBurstSize);
	auto burstInfo
		= ConvertTokensToBurstSize(replicatedPackets, maxPossibleBurstSize, availableTokens);
	_rateLimiter.SetProcessedTokens(burstInfo.usedTokens);
	return burstInfo.burstSize;
}

Replicator::BurstInfo Replicator::ConvertTokensToBurstSize(
	uint64_t replicatedPackets,
	uint64_t maxPossibleBurstSize,
	uint64_t availableTokens)
{
	if (std::holds_alternative<Config::RateLimitPps>(_rateLimiterConfig)) {
		const uint64_t size = std::min(maxPossibleBurstSize, availableTokens);
		return {size, size};
	}

	if (std::holds_alternative<Config::RateLimitMbps>(_rateLimiterConfig)) {
		uint64_t tokensSum = 0;
		for (uint64_t idx = 0; idx < maxPossibleBurstSize; idx++) {
			uint64_t packetQueueIndex = GetIndexToPacketQueue(idx + replicatedPackets);
			uint64_t packetTokensSize = _packetQueue[packetQueueIndex]->dataLen;
			if (tokensSum + packetTokensSize > availableTokens) {
				return {idx, tokensSum};
			}
			tokensSum += packetTokensSize;
		}
		return {maxPossibleBurstSize, tokensSum};
	}

	if (std::holds_alternative<Config::RateLimitTimeUnit>(_rateLimiterConfig)) {
		uint64_t tokensSum = 0;
		for (uint64_t idx = 0; idx < maxPossibleBurstSize; idx++) {
			uint64_t packetQueueIndex = GetIndexToPacketQueue(idx + replicatedPackets);
			uint64_t packetTokensSize
				= _packetQueue[packetQueueIndex]->timestamp - _lastPacketTimestamp;
			if (tokensSum + packetTokensSize > availableTokens) {
				return {idx, tokensSum};
			}
			_lastPacketTimestamp = _packetQueue[packetQueueIndex]->timestamp;
			tokensSum += packetTokensSize;
		}
		return {maxPossibleBurstSize, tokensSum};
	}

	return {maxPossibleBurstSize, 0};
}

uint64_t Replicator::GetMinimumRequiredTokens(uint64_t replicatedPackets)
{
	if (std::holds_alternative<Config::RateLimitPps>(_rateLimiterConfig)) {
		return 1;
	}

	size_t packetQueueIndex = GetIndexToPacketQueue(replicatedPackets);

	if (std::holds_alternative<Config::RateLimitMbps>(_rateLimiterConfig)) {
		return _packetQueue[packetQueueIndex]->dataLen;
	}

	if (std::holds_alternative<Config::RateLimitTimeUnit>(_rateLimiterConfig)) {
		return _packetQueue[packetQueueIndex]->timestamp - _lastPacketTimestamp;
	}
	return 0;
}

uint64_t Replicator::GetIndexToPacketQueue(uint64_t replicatedPackets) const noexcept
{
	return replicatedPackets / _loopAvailableReplicationUnits.size();
}

uint64_t Replicator::GetReplicationUnitIndex(uint64_t replicatedPackets) const noexcept
{
	return replicatedPackets % _loopAvailableReplicationUnits.size();
}

} // namespace replay
