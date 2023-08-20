/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator engine
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
 * The tradeoff _might_ be less cryptographically secure output, but for our purposes that is not an
 * issue
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

} // namespace generator
