/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv6addressgenerator.h"

namespace generator {

IPv6AddressGenerator::IPv6AddressGenerator(IPv6Address netIp, uint8_t prefixLen)
	: _gen(
		  PrefixedGenerator(std::vector<uint8_t>(netIp.toBytes(), netIp.toBytes() + 16), prefixLen))
{
	if (prefixLen > 0 && !netIp.isValid()) {
		throw std::invalid_argument("invalid IPv6AddressGenerator net address");
	}
}

IPv6Address IPv6AddressGenerator::Generate()
{
	return IPv6Address(_gen.Generate().data());
}

} // namespace generator
