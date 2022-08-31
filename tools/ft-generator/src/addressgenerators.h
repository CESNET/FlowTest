/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generators of addresses used in flows
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <pcapplusplus/IpAddress.h>
#include <pcapplusplus/MacAddress.h>

#include <array>

namespace generator {

using MacAddress = pcpp::MacAddress;
using IPv4Address = pcpp::IPv4Address;
using IPv6Address = pcpp::IPv6Address;

/**
 * @brief Generator of unique MAC/IP addresses based on a pseudorandom generator
 *
 */
class AddressGenerators {
public:
	/**
	 * @brief Construct a new Address Generators object
	 *
	 * @param seed  The seed value of the pseudorandom seed generator
	 */
	AddressGenerators(uint32_t seed = 1);

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
	uint32_t _capacity = 0;
	uint32_t _state;
	uint32_t _seedState;

	uint32_t NextValue();
	void NextSeed();
};

} // namespace generator
