/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "prefixedgenerator.h"

#include <pcapplusplus/MacAddress.h>

namespace generator {

using MacAddress = pcpp::MacAddress;

/**
 * @brief Generator of Mac addresses
 */
class MacAddressGenerator {
public:
	/**
	 * @brief Construct a new Mac Address Generator object
	 *
	 * @param baseAddr   The base address to generate the addresses from, respectively the prefix
	 * @param prefixLen  The prefix length
	 * @param seed       The seed
	 */
	MacAddressGenerator(MacAddress baseAddr, uint8_t prefixLen, const std::bitset<128>& seed);

	/**
	 * @brief Generate a Mac address
	 *
	 * @return MacAddress
	 */
	MacAddress Generate();

private:
	PrefixedGenerator _gen;
	uint8_t _prefixLen;
};

} // namespace generator
