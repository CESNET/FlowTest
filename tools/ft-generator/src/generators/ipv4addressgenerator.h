/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief IPv4 address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "prefixedgenerator.h"

#include <pcapplusplus/IpAddress.h>

namespace generator {

using IPv4Address = pcpp::IPv4Address;

/**
 * @brief Generator of IPv4 addresses
 */
class IPv4AddressGenerator {
public:
	/**
	 * @brief Construct a new IPv4AddressGenerator object
	 *
	 * @param netIp      The network address to generate the addresses from
	 * @param prefixLen  The prefix length
	 */
	IPv4AddressGenerator(IPv4Address netIp, uint8_t prefixLen);

	/**
	 * @brief Generate an IPv4 address
	 *
	 * @return The address
	 */
	IPv4Address Generate();

private:
	PrefixedGenerator _gen;
};

} // namespace generator
