/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "randomgenerator.h"

#include <algorithm>
#include <ctime>

namespace generator {

static inline constexpr float FloatFromBits(uint32_t i) noexcept
{
	return (i >> 8) * 0x1.0p-24f;
}

static inline constexpr double DoubleFromBits(uint64_t i) noexcept
{
	return (i >> 11) * 0x1.0p-53;
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

std::string RandomGenerator::RandomString(std::size_t length, std::string_view charset)
{
	std::string s;
	s.reserve(length);
	for (std::size_t i = 0; i < length; i++) {
		s.push_back(RandomChoice(charset));
	}
	return s;
}

std::vector<uint8_t> RandomGenerator::RandomBytes(std::size_t length)
{
	std::vector<uint8_t> data(length);
	std::generate(data.begin(), data.end(), [&]() { return RandomUInt(0, 255); });
	return data;
}

} // namespace generator
