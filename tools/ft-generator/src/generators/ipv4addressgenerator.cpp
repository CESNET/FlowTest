/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv4addressgenerator.h"

#include "common.h"

namespace generator {

IPv4AddressGenerator::IPv4AddressGenerator(
	IPv4Address netIp, uint8_t prefixLen, const std::bitset<128>& seed) :
	_gen(PrefixedGenerator(bytesToBitset(netIp.toBytes(), 4), prefixLen, 32, seed))
{
	if (prefixLen > 0 && !netIp.isValid()) {
		throw std::invalid_argument("invalid net address");
	}
}

IPv4Address IPv4AddressGenerator::Generate()
{
	uint8_t bytes[4];
	bitsetToBytes(_gen.Generate(), bytes, 4);
	return IPv4Address(bytes);
}

} // namespace generator
