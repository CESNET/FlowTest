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
 * @brief A generator of MAC addresses
 */
class MacAddressGenerator {
public:
	/**
	 * @brief Generate next address
	 *
	 * @return The address
	 */
	MacAddress Generate();

private:
	std::array<uint8_t, 6> _currentAddr = {0};
};

/**
 * @brief A generator of IPv4 addresses
 */
class IPv4AddressGenerator {
public:
	/**
	 * @brief Generate next address
	 *
	 * @return The address
	 */
	IPv4Address Generate();

private:
	std::array<uint8_t, 4> _currentAddr = {0};
};

/**
 * @brief A generator of IPv6 addresses
 */
class IPv6AddressGenerator {
public:
	/**
	 * @brief Generate next address
	 *
	 * @return The address
	 */
	IPv6Address Generate();

private:
	std::array<uint8_t, 16> _currentAddr = {0};
};

/**
 * @brief A container for all the address generators
 */
struct AddressGenerators {
	MacAddressGenerator _mac;
	IPv4AddressGenerator _ipv4;
	IPv6AddressGenerator _ipv6;
};

} // namespace generator
