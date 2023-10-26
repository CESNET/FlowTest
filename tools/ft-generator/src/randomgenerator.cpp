/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "randomgenerator.h"
#include "logger.h"

#include <algorithm>
#include <cassert>
#include <ctime>
#include <memory>
#include <numeric>
#include <vector>

namespace generator {

static std::unique_ptr<RandomGenerator> theInstance;

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

void RandomGenerator::InitInstance(std::optional<uint64_t> seed)
{
	uint64_t seedValue = seed ? *seed : std::time(nullptr);

	ft::LoggerGet("RandomGenerator")->info("Seed: {}", seedValue);
	theInstance.reset(new RandomGenerator(seedValue));
}

RandomGenerator& RandomGenerator::GetInstance()
{
	return *theInstance.get();
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

static void GenerateNormWeights(
	RandomGenerator& rng,
	std::vector<double>::iterator begin,
	std::vector<double>::iterator end)
{
	double sum = 0;

	for (auto it = begin; it != end; it++) {
		*it = rng.RandomDouble();
		sum += *it;
	}

	for (auto it = begin; it != end; it++) {
		*it /= sum;
	}
}

static uint64_t WeightedDistribute(
	uint64_t amount,
	std::vector<uint64_t>::iterator valuesBegin,
	std::vector<uint64_t>::iterator valuesEnd,
	std::vector<double>::iterator weightsBegin,
	uint64_t max)
{
	uint64_t remainder = amount;
	auto weight = weightsBegin;

	for (auto value = valuesBegin; value != valuesEnd; value++, weight++) {
		uint64_t part = std::min<uint64_t>(max - *value, amount * *weight);
		*value += part;

		assert(remainder >= part);
		remainder -= part;
	}

	return remainder;
}

static uint64_t SimpleDistribute(
	uint64_t amount,
	std::vector<uint64_t>::iterator begin,
	std::vector<uint64_t>::iterator end,
	uint64_t max)
{
	for (auto value = begin; value != end && amount > 0; value++) {
		uint64_t part = std::min<uint64_t>(max - *value, amount);
		*value += part;
		amount -= part;
	}
	return amount;
}

std::vector<uint64_t> RandomGenerator::RandomlyDistribute(
	uint64_t amount,
	std::size_t numValues,
	uint64_t min,
	uint64_t max)
{
	if (min > max || numValues * min > amount || numValues * max < amount) {
		throw std::logic_error("distribution impossible with the constraints provided");
	}

	std::vector<uint64_t> values(numValues, min);
	amount -= numValues * min;

	std::vector<double> weights(numValues);

	auto begin = values.begin();
	auto end = values.end();

	while (begin < end && amount > 0) {
		GenerateNormWeights(*this, weights.begin(), weights.end());

		uint64_t oldAmount = amount;
		amount = WeightedDistribute(amount, begin, end, weights.begin(), max);

		if (oldAmount == amount) {
			amount = SimpleDistribute(amount, begin, end, max);
			break;
		}

		end = std::partition(begin, end, [=](auto v) { return v < max; });
		weights.resize(std::distance(begin, end));
	}

	(void) amount;
	assert(amount == 0);

	Shuffle(values);

	return values;
}

std::vector<uint8_t> RandomGenerator::RandomBytes(std::size_t length)
{
	std::vector<uint8_t> data(length);
	std::generate(data.begin(), data.end(), [&]() { return RandomUInt(0, 255); });
	return data;
}

} // namespace generator
