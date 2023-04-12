/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Declaration of the RateLimiter class, which provides rate limiting functionality.
 *
 * RateLimiter is a class that provides rate limiting functionality based on tokens. It inherits
 * from TokenConverter, which provides time conversion between tokens and nanoseconds. It allows
 * users to limit the number of tokens that can be processed per second by specifying a limit on the
 * number of tokens per second. If the rate limit is exceeded, the Limit() method will block until
 * the expected outgoing time.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <ctime>

namespace replay {

/**
 * @brief A utility class for converting between tokens and timestamps.
 *
 * This class provides methods for converting between a timestamp represented as a 64-bit integer in
 * nanoseconds and a number of tokens per second. The precision of the conversion is determined by
 * the number of tokens per second, which is set in the constructor and can be modified using the
 * SetTokensRate method.
 */
class TokenConverter {
public:
	/**
	 * @brief Alias for a 64-bit integer representing a nanosecond timestamp.
	 */
	using NanosecTime = uint64_t;

	/**
	 * @brief Constructor for TokenConverter class.
	 *
	 * Initializes the TokenConverter object with the given number of tokens per second.
	 *
	 * @param tokensPerSec The number of tokens per second.
	 */
	explicit TokenConverter(uint64_t tokensPerSec) noexcept;

	/**
	 * @brief Sets the number of tokens per second for this TokenConverter object.
	 *
	 * @param tokensPerSec The new number of tokens per second.
	 */
	void SetTokensRate(uint64_t tokensPerSec) noexcept;

	/**
	 * @brief Converts the given number of tokens into a nanosecond timestamp.
	 *
	 * @param tokens The number of tokens to convert into a nanosecond timestamp.
	 * @return The nanosecond timestamp.
	 */
	NanosecTime TokensToTimeDelta(uint64_t tokens) const noexcept;

	/**
	 * @brief Converts the given timespec structure into the corresponding number of tokens.
	 *
	 * @param ts A timespec structure representing a timestamp.
	 * @return The corresponding number of tokens.
	 */
	uint64_t TimeDeltaToTokens(timespec ts) const noexcept;

	/**
	 * @brief Converts the given nanosecond timestamp into the corresponding number of tokens.
	 *
	 * @param timeDelta The nanosecond timestamp to convert into tokens.
	 * @return The corresponding number of tokens.
	 */
	uint64_t TimeDeltaToTokens(NanosecTime timeDelta) const noexcept;

private:
	int GetPrecisionShift(uint64_t tokensPerSec) const noexcept;

	uint64_t _tokensPerSec;
	int _precisionShift;
};

/**
 * @brief Class for limiting the rate of tokens processed per second.
 *
 * This class inherits from TokenConverter and implements a token bucket algorithm to limit the rate
 * of tokens processed per second. The user can set the token limit per second, and the rate limiter
 * will delay processing if the number of tokens in the bucket exceeds the limit. If processing is
 * too slow, the gap is reduced to the token rate limit size to ensure that the future processing
 * rate does not exceed the limit.
 */
class RateLimiter : TokenConverter {
public:
	/**
	 * @brief Constructs a new `RateLimiter` instance with the specified token rate limit.
	 *
	 * If @p tokensLimitPerSecond is 0, rate limiting is turned off.
	 *
	 * @param tokensLimitPerSecond The maximum number of tokens that can be processed in one second.
	 */
	explicit RateLimiter(uint64_t tokensLimitPerSecond = 0);

	/**
	 * @brief Sets the maximum number of tokens allowed per second.
	 *
	 * If @p tokensLimitPerSecond is 0, rate limiting is turned off.
	 *
	 * @param tokensLimitPerSecond The maximum number of tokens that can be processed in one second.
	 */
	void SetLimit(uint64_t tokensLimitPerSecond) noexcept;

	/**
	 * @brief Limits the rate at which tokens are processed.
	 *
	 * This function limits the rate of token processing based on the token limit per second set by
	 * the user. If the token limit has not been set (or set to zero), this function does nothing.
	 * The function will delay processing if the number of tokens in the bucket exceeds the limit.
	 * If processing is too slow, the gap is reduced to the token rate limit size to ensure that the
	 * future processing rate does not exceed the limit.
	 *
	 * @param tokensToProcess The number of tokens to process.
	 */
	void Limit(uint64_t tokensToProcess) noexcept;

private:
	void Reset() noexcept;
	void InitStartTime() noexcept;
	NanosecTime GetCurrentTimeDelta() const noexcept;

	uint64_t _tokenslimitPerSecond;
	uint64_t _tokensInBucket;
	NanosecTime _startTime;
	bool _isStartTimeInitialized;
};

} // namespace replay
