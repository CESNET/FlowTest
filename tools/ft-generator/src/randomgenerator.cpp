/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "randomgenerator.h"

#include <ctime>

namespace generator {

static inline constexpr uint64_t Rotl(const uint64_t x, int k) noexcept
{
	return (x << k) | (x >> (64 - k));
}

static inline constexpr float FloatFromBits(uint32_t i) noexcept
{
	return (i >> 8) * 0x1.0p-24f;
}

static inline constexpr double DoubleFromBits(uint64_t i) noexcept
{
	return (i >> 11) * 0x1.0p-53;
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

RandomGenerator::RandomGenerator(uint64_t seed)
	: _engine(seed)
{
}

RandomGenerator& RandomGenerator::GetInstance()
{
	static RandomGenerator instance(std::time(nullptr));
	return instance;
}

uint64_t RandomGenerator::RandomUInt()
{
	return _engine.Next();
}

uint64_t RandomGenerator::RandomUInt(uint64_t min, uint64_t max)
{
	return min + RandomUInt() % (max - min + 1);
}

float RandomGenerator::RandomFloat()
{
	return FloatFromBits(RandomUInt());
}

double RandomGenerator::RandomDouble()
{
	return DoubleFromBits(RandomUInt());
}

double RandomGenerator::RandomDouble(double min, double max)
{
	return min + RandomDouble() * (max - min);
}

} // namespace generator
