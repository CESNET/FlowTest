/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Prefixed generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "lfsr.h"

#include <cstdint>
#include <vector>

namespace generator {

/**
 * @brief Generator for series of bits with a specified prefix
 */
class PrefixedGenerator {
public:
	/**
	 * @brief Construct a new Prefixed Generator object
	 *
	 * @param base       The base address
	 * @param prefixLen  Number of prefix bits in base address
	 *
	 * @note The length of the generated addresses is determined by the length of the
	 *       base address vector.
	 */
	PrefixedGenerator(const std::vector<uint8_t>& base, unsigned int prefixLen);

	/**
	 * @brief Generate a series of bits with the desired length and prefix
	 *
	 * @return The generated bits. The size of the returned vector is equal to the size of the
	 *         provided base address.
	 */
	std::vector<uint8_t> Generate();

private:
	std::vector<uint8_t> _base;
	unsigned int _prefixLen;
	unsigned int _totalLen;
	Lfsr _lfsr;
};

} // namespace generator
