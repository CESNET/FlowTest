/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief LFSR generator implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
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
	 * @param nBits  The number of state register bits, at most 128
	 *
	 * @throw std::invalid_argument in case nBits is an invalid number of bits
	 */
	Lfsr(unsigned int nBits);

	/**
	 * @brief Generate the next bit
	 */
	void Next();

	/**
	 * @brief Get the state of the shift register
	 */
	const std::vector<bool>& GetState() const { return _state; }

private:
	std::vector<int> _taps; //< The tap mask used for calculation of the next bit
	std::vector<bool> _zeroState; //< State of all zeros
	std::vector<bool> _initState; //< The initial shift register state
	std::vector<bool> _state; //< The shift register state
};

} // namespace generator
