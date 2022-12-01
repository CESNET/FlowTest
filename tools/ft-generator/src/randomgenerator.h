/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace generator {

/**
 * @brief A SplitMix64 random number generator
 *
 * Based on https://prng.di.unimi.it/splitmix64.c
 *
 * Used for generation of seed for the Xoshiro256PlusPlusGenerator
 */
class SplitMix64Generator {
public:
	/**
	 * @brief Construct a new SplitMix64 Generator object
	 *
	 * @param seed  The generator seed
	 */
	SplitMix64Generator(uint64_t seed);

	/**
	 * @brief Generate the next value
	 *
	 * @return uint64_t  The value
	 */
	uint64_t Next();

private:
	uint64_t _state; // The state can be seeded with any value.
};

/**
 * @brief A fast and efficient Xoshiro256++ random number generator
 *
 * Based on https://prng.di.unimi.it/xoshiro256plusplus.c
 *
 * The tradeoff _might_ be less cryptographically secure output, but for our purposes that is not an issue
 */
class Xoshiro256PlusPlusGenerator {
public:
	/**
	 * @brief Construct a new Xoshiro256++ Generator object
	 *
	 * @param seed  The generator seed
	 */
	Xoshiro256PlusPlusGenerator(uint64_t seed);

	/**
	 * @brief Generate the next value
	 *
	 * @return uint64_t  The value
	 */
	uint64_t Next();

private:
	uint64_t _state[4];
};


/**
 * @brief A random number generator
 *
 * Used instead of std::random for better performance
 *
 * Uses the Xoshiro256++ as its engine and provides various value generation functions on top
 */
class RandomGenerator {
public:
	/**
	 * @brief Get the Random Generator instance
	 *
	 * @return RandomGenerator&
	 */
	static RandomGenerator& GetInstance();

	/**
	 * @brief Generate a random 64 bit unsigned integer
	 *
	 * @return uint64_t  The generated random value
	 */
	uint64_t RandomUInt();

	/**
	 * @brief Generate a random 64 bit unsigned integer in specified range
	 *
	 * @param min  The minimum possible value
	 * @param max  The maximum possible value (including!)
	 * @return uint64_t  The generated random value
	 */
	uint64_t RandomUInt(uint64_t min, uint64_t max);

	/**
	 * @brief Generate a random float in range from 0.0 to 1.0
	 *
	 * @return float  The generated random value
	 */
	float RandomFloat();

	/**
	 * @brief Generate a random double in range from 0.0 to 1.0
	 *
	 * @return double  The generated random value
	 */
	double RandomDouble();

	/**
	 * @brief Generate a random double in specified range
	 *
	 * @param min  The minimum possible value
	 * @param max  The maximum possible value
	 * @return double  The generated random value
	 */
	double RandomDouble(double min, double max);

private:
	Xoshiro256PlusPlusGenerator _engine;

	/**
	 * @brief Construct a new Random Generator object
	 *
	 * @param seed  The seed for the underlying generator
	 */
	RandomGenerator(uint64_t seed);

	/**
	 * Disable copy and move constructors
	 */
	RandomGenerator(const RandomGenerator&) = delete;
	RandomGenerator(RandomGenerator&&) = delete;
	RandomGenerator& operator=(const RandomGenerator&) = delete;
	RandomGenerator& operator=(RandomGenerator&&) = delete;
};

} // namespace generator
