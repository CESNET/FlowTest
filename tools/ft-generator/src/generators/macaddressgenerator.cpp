/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "macaddressgenerator.h"

#include "common.h"

namespace generator {

MacAddressGenerator::MacAddressGenerator(
	MacAddress baseAddr, uint8_t prefixLen, const std::bitset<128>& seed) :
	_gen(PrefixedGenerator(bytesToBitset(baseAddr.getRawData(), 6), prefixLen, 48, seed))
{
	if (prefixLen > 0 && !baseAddr.isValid()) {
		throw std::invalid_argument("invalid MacAddressGenerator base address");
	}
}

MacAddress MacAddressGenerator::Generate()
{
	uint8_t bytes[6];
	bitsetToBytes(_gen.Generate(), bytes, 6);
	return MacAddress(bytes);
}

} // namespace generator
