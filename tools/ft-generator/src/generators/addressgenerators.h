/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generators of addresses used in flows
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../config/ipv4.h"
#include "../config/ipv6.h"
#include "../config/mac.h"
#include "ipv4addressgenerator.h"
#include "ipv6addressgenerator.h"
#include "macaddressgenerator.h"
#include "multirangegenerator.h"

namespace generator {

/**
 * @brief Generator of unique MAC/IP addresses based on a pseudorandom generator
 */
class AddressGenerators {
public:
	/**
	 * @brief Construct a new Address Generators object
	 *
	 * @param seed  The seed value of the pseudorandom seed generator
	 */
	AddressGenerators(
		const std::vector<config::IPv4AddressRange>& ipv4Config,
		const std::vector<config::IPv6AddressRange>& ipv6Config,
		const std::vector<config::MacAddressRange>& macConfig);

	/**
	 * @brief Generate a MAC address
	 *
	 * @return MacAddress
	 */
	MacAddress GenerateMac();

	/**
	 * @brief Generate an IPv4 address
	 *
	 * @return IPv4Address
	 */
	IPv4Address GenerateIPv4();

	/**
	 * @brief Generate an IPv6 address
	 *
	 * @return IPv6Address
	 */
	IPv6Address GenerateIPv6();

private:
	MultiRangeGenerator<IPv4AddressGenerator, IPv4Address> _ipv4;
	MultiRangeGenerator<IPv6AddressGenerator, IPv6Address> _ipv6;
	MultiRangeGenerator<MacAddressGenerator, MacAddress> _mac;
};

} // namespace generator
