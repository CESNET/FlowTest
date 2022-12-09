/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief LFSR generator implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <bitset>
#include <vector>

namespace generator {

/**
 * @brief Implementation of a LFSR (Linear-Feedback Shift Register) generator
 *
 * Used for generation of adresses
 *
 * Allows for better control of the period of the generator compared to e.g. LCG
 */
class Lfsr {
public:
	/**
	 * @brief Construct a new Lfsr object
	 *
	 * @param nBits  The number of bits
	 * @param seed   The initial seed, respectively the shift register state
	 *
	 * @throw std::invalid_argument in case nBits is an invalid number of bits
	 */
	Lfsr(unsigned int nBits, const std::bitset<128> &seed);

	/**
	 * @brief Generate the next 128 bit value
	 *
	 * @return The generated bits
	 *
	 * @warning Despite the return value being 128 bits, bits on positions
	 *          higher than the specified totalLen have undefined value!
	 */
	std::bitset<128> Next();

private:
	unsigned int _nBits;         //< The number of bits
	std::vector<uint64_t> _taps; //< The taps used for calculation of the next bit
	std::bitset<128> _state;     //< The shift register state

	void Shift();
};

} // namespace generator
