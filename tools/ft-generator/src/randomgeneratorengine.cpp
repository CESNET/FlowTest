/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator engine
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "randomgeneratorengine.h"

namespace generator {

static inline constexpr uint64_t Rotl(const uint64_t x, int k) noexcept
{
	return (x << k) | (x >> (64 - k));
}

SplitMix64Generator::SplitMix64Generator(uint64_t seed)
	: _state(seed)
{
}

uint64_t SplitMix64Generator::Next()
{
	uint64_t z = (_state += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

Xoshiro256PlusPlusGenerator::Xoshiro256PlusPlusGenerator(uint64_t seed)
{
	SplitMix64Generator gen(seed);
	_state[0] = gen.Next();
	_state[1] = gen.Next();
	_state[2] = gen.Next();
	_state[3] = gen.Next();
}

uint64_t Xoshiro256PlusPlusGenerator::Next()
{
	const uint64_t result = Rotl(_state[0] + _state[3], 23) + _state[0];

	const uint64_t t = _state[1] << 17;

	_state[2] ^= _state[0];
	_state[3] ^= _state[1];
	_state[1] ^= _state[2];
	_state[0] ^= _state[3];

	_state[2] ^= t;

	_state[3] = Rotl(_state[3], 45);

	return result;
}

} // namespace generator
