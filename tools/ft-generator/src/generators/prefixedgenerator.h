/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Prefixed generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "lfsr.h"

#include <bitset>
#include <cstdint>

namespace generator {

/**
 * @brief Generator for series of bits with a specified prefix
 */
class PrefixedGenerator {
public:
	/**
	 * @brief Construct a new Prefixed Generator object
	 *
	 * @param prefix     The prefix bits
	 * @param prefixLen  The length of the prefix
	 * @param totalLen   Total length of the generated addresses
	 * @param seed       The seed
	 */
	PrefixedGenerator(
		const std::bitset<128>& prefix,
		uint8_t prefixLen,
		uint8_t totalLen,
		const std::bitset<128>& seed);

	/**
	 * @brief Generate a series of bits with the desired length and prefix
	 *
	 * @return The generated bits
	 *
	 * @warning Despite the return value being 128 bits, bits on positions
	 *          higher than the specified totalLen have undefined value!
	 */
	std::bitset<128> Generate();

private:
	std::bitset<128> _prefix;
	uint8_t _prefixLen;
	Lfsr _lfsr;
};

} // namespace generator
