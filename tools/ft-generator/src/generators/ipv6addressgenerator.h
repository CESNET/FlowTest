/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv6 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "prefixedgenerator.h"

#include <pcapplusplus/IpAddress.h>

namespace generator {

using IPv6Address = pcpp::IPv6Address;

/**
 * @brief Generator of IPv6 addresses
 */
class IPv6AddressGenerator {
public:
	/**
	 * @brief Construct a new IPv6AddressGenerator object
	 *
	 * @param netIp      The network address to generate the addresses from
	 * @param prefixLen  The prefix length
	 */
	IPv6AddressGenerator(IPv6Address netIp, uint8_t prefixLen);

	/**
	 * @brief Generate an IPv6 address
	 *
	 * @return The address
	 */
	IPv6Address Generate();

private:
	PrefixedGenerator _gen;
};

} // namespace generator
