/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Implementation of the RateLimiter class, which provides rate limiting functionality.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rateLimiter.hpp"

#include <chrono>
#include <thread>

namespace replay {

using namespace std::chrono_literals;
static constexpr uint64_t NANOSEC_IN_SEC = std::chrono::nanoseconds(1s).count();

TokenConverter::TokenConverter(uint64_t tokensPerSec) noexcept
{
	SetTokensRate(tokensPerSec);
}

void TokenConverter::SetTokensRate(uint64_t tokensPerSec) noexcept
{
	_tokensPerSec = tokensPerSec;
	_precisionShift = GetPrecisionShift(tokensPerSec);
}

TokenConverter::NanosecTime TokenConverter::TokensToTimeDelta(uint64_t tokens) const noexcept
{
	const uint64_t secs = tokens / _tokensPerSec;
	uint64_t nanosecs;

	nanosecs = tokens % _tokensPerSec;
	nanosecs >>= _precisionShift;
	nanosecs *= NANOSEC_IN_SEC;
	nanosecs /= _tokensPerSec;
	nanosecs <<= _precisionShift;

	return secs * NANOSEC_IN_SEC + nanosecs;
}

uint64_t TokenConverter::TimeDeltaToTokens(timespec ts) const noexcept
{
	uint64_t tokens = _tokensPerSec * ts.tv_sec;
	uint64_t fraction;

	fraction = _tokensPerSec >> _precisionShift;
	fraction *= ts.tv_nsec;
	fraction /= NANOSEC_IN_SEC;
	fraction <<= _precisionShift;

	tokens += fraction;
	return tokens;
}

uint64_t TokenConverter::TimeDeltaToTokens(TokenConverter::NanosecTime timeDelta) const noexcept
{
	const timespec ts = {time_t(timeDelta / NANOSEC_IN_SEC), long(timeDelta % NANOSEC_IN_SEC)};
	return TimeDeltaToTokens(ts);
}

/**
 * @brief Calculate the token calculation accuracy reduction
 *
 * For @p tokensPerSec greater than the number of nanoseconds in one second (i.e., 1e9), overflow
 * may occur when converting a portion of a nanosecond between tokens and a timestamp and vice
 * versa. For example, for tokens representing the number of bytes per second on a 100 Gb/s link,
 * the value would be tokensPerSec = 13,421,772,800. When converting the timestamp to tokens, the
 * formula "tokensPerSec * nanoseconds / 1e9" is used for the nanosecond part, but for the
 * nanosecond value = 999,999,999 this will cause a 64-bit variable overflow. The function will
 * calculate the smallest necessary precision reduction to avoid overflow.
 *
 * @param[in] tokensPerSec Tokens per second
 * @return Number of bit shift positions of the numerator.
 */
int TokenConverter::GetPrecisionShift(uint64_t tokensPerSec) const noexcept
{
	int shift = 0;

	while (tokensPerSec > NANOSEC_IN_SEC) {
		tokensPerSec >>= 1;
		shift++;
	}

	return shift;
}

RateLimiter::RateLimiter(uint64_t tokensLimitPerSecond)
	: TokenConverter(tokensLimitPerSecond)
{
	SetLimit(tokensLimitPerSecond);
}

void RateLimiter::SetLimit(uint64_t tokensLimitPerSecond) noexcept
{
	Reset();
	SetTokensRate(tokensLimitPerSecond);

	_tokenslimitPerSecond = tokensLimitPerSecond;
}

void RateLimiter::Reset() noexcept
{
	_tokensInBucket = 0;
	_startTime = 0;
	_isStartTimeInitialized = false;
}

void RateLimiter::Limit(uint64_t tokensToProcess) noexcept
{
	if (!_tokenslimitPerSecond) {
		return;
	}

	if (!_isStartTimeInitialized) {
		InitStartTime();
	}

	auto currentTimeDelta = GetCurrentTimeDelta();
	auto expectedOutgouingTime = TokensToTimeDelta(_tokensInBucket);

	// On time, waiting till expected outgoing time
	if (currentTimeDelta < expectedOutgouingTime) {
		std::this_thread::sleep_for(
			std::chrono::nanoseconds(expectedOutgouingTime - currentTimeDelta));
	} else { // late, check the token gap size in bucket
		uint64_t expectedTokensInBucket = TimeDeltaToTokens(currentTimeDelta);

		/**
		 * If the difference between the number of expected tokens in bucket and current tokens in
		 * bucket is greater than the token rate limit, it means processing is too slow, then the
		 * gap is reduced to the token rate limit size.
		 *
		 * This will ensure that in the future we do not process more tokens per second than the
		 * limit.
		 */
		if (expectedTokensInBucket - _tokensInBucket > _tokenslimitPerSecond) {
			_tokensInBucket = expectedTokensInBucket - _tokenslimitPerSecond;
		}
	}
	_tokensInBucket += tokensToProcess;
}

void RateLimiter::InitStartTime() noexcept
{
	_startTime = GetCurrentTimeDelta();
	_isStartTimeInitialized = true;
}

RateLimiter::NanosecTime RateLimiter::GetCurrentTimeDelta() const noexcept
{
	struct timespec currentTime;
	clock_gettime(CLOCK_MONOTONIC, &currentTime);
	return (currentTime.tv_sec * NANOSEC_IN_SEC + currentTime.tv_nsec) - _startTime;
}

} // namespace replay
