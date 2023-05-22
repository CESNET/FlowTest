/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "outputQueue.hpp"

namespace replay {

void OutputQueueStats::UpdateTime() noexcept
{
	timespec now;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
	transmitEndTime = now;

	if (!transmitStartTime.tv_sec && !transmitStartTime.tv_nsec) {
		transmitStartTime = now;
	}
}

OutputQueue::OutputQueue()
	: _rateLimitType(std::monostate())
	, _rateLimiter(0)
{
}

void OutputQueue::RateLimit(uint64_t packetsCount, uint64_t sumOfPacketsLength)
{
	uint64_t tokensCount = 0;
	if (std::holds_alternative<Config::RateLimitPps>(_rateLimitType)) {
		tokensCount = packetsCount;
	} else if (std::holds_alternative<Config::RateLimitMbps>(_rateLimitType)) {
		tokensCount = sumOfPacketsLength;
	}

	_rateLimiter.Limit(tokensCount);
}

void OutputQueue::SetRateLimiter(Config::RateLimit rateLimitConfig)
{
	if (std::holds_alternative<std::monostate>(rateLimitConfig)) {
		_rateLimiter.SetLimit(0);
	} else if (std::holds_alternative<Config::RateLimitPps>(rateLimitConfig)) {
		_rateLimiter.SetLimit(std::get<Config::RateLimitPps>(rateLimitConfig).value);
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimitConfig)) {
		_rateLimiter.SetLimit(
			std::get<Config::RateLimitMbps>(rateLimitConfig).ConvertToBytesPerSecond());
	} else {
		_logger->critical("OutputQueue::SetRateLimiter() has failed.");
		throw std::logic_error("RateLimit config type is not implemented");
	}

	_rateLimitType = rateLimitConfig;
}

} // namespace replay
