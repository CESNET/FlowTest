/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipv4addressgenerator.h"

namespace generator {

IPv4AddressGenerator::IPv4AddressGenerator(IPv4Address netIp, uint8_t prefixLen)
	: _gen(PrefixedGenerator(std::vector<uint8_t>(netIp.toBytes(), netIp.toBytes() + 4), prefixLen))
{
	if (prefixLen > 0 && !netIp.isValid()) {
		throw std::invalid_argument("invalid net address");
	}
}

IPv4Address IPv4AddressGenerator::Generate()
{
	return IPv4Address(_gen.Generate().data());
}

} // namespace generator
