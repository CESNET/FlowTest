/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv6addressgenerator.h"

#include "common.h"

namespace generator {

IPv6AddressGenerator::IPv6AddressGenerator(
	IPv6Address netIp,
	uint8_t prefixLen,
	const std::bitset<128>& seed)
	: _gen(PrefixedGenerator(BytesToBitset(netIp.toBytes(), 16), prefixLen, 128, seed))
{
	if (prefixLen > 0 && !netIp.isValid()) {
		throw std::invalid_argument("invalid IPv6AddressGenerator net address");
	}
}

IPv6Address IPv6AddressGenerator::Generate()
{
	uint8_t bytes[16];
	BitsetToBytes(_gen.Generate(), bytes, 16);
	return IPv6Address(bytes);
}

} // namespace generator
