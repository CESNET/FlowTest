/**
 * @file
 * @author Lukáš Huták <hutak@cesnet.cz>
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief IP address and network representation and manipulation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ft::analyzer {

/**
 * @union IPAddr
 * @brief Represents an IP address (IPv4 or IPv6).
 */
union IPAddr {
	std::array<uint8_t, 16> u8;
	std::array<uint16_t, 8> u16;
	std::array<uint32_t, 4> u32;
	std::array<uint64_t, 2> u64;

	/**
	 * @brief Constructs a zero-initialized IP address.
	 */
	IPAddr();

	/**
	 * @brief Constructs an IP address from a string.
	 * @param[in] ip String representation of the IP address.
	 * @throw std::invalid_argument If the string cannot be converted to an IP address.
	 */
	explicit IPAddr(const std::string& ip);

	/**
	 * @brief Checks if the IP address is IPv4.
	 * @return True if the address is IPv4, false otherwise.
	 */
	bool IsIPv4() const;

	/**
	 * @brief Checks if the IP address is IPv6.
	 * @return True if the address is IPv6, false otherwise.
	 */
	bool IsIPv6() const;

	/**
	 * @brief Converts the IP address to its string representation.
	 * @return String representation of the IP address.
	 * @throw std::runtime_error If the conversion fails.
	 */
	std::string ToString() const;

	bool operator==(const IPAddr& other) const;
	bool operator!=(const IPAddr& other) const;
	bool operator<(const IPAddr& other) const;
	bool operator<=(const IPAddr& other) const;
	bool operator>(const IPAddr& other) const;
	bool operator>=(const IPAddr& other) const;
};

static_assert(sizeof(IPAddr) == 16U, "IPAddr size must be 16 bytes");

/**
 * @class IPNetwork
 * @brief Represents an IP network with a prefix length.
 */
class IPNetwork {
public:
	/**
	 * @brief Constructs an IP network from an IP address and prefix length.
	 * @param[in] ip String representation of the IP address.
	 * @param[in] prefixLen Prefix length of the network.
	 */
	IPNetwork(const std::string& ip, uint8_t prefixLen);

	/**
	 * @brief Constructs an IP network from an IP address with a default prefix length.
	 * @param[in] ip String representation of the IP address.
	 * @details The default prefix length is 32 for IPv4 and 128 for IPv6.
	 */
	explicit IPNetwork(const std::string& ip);

	/**
	 * @brief Checks if an IP address is within the network.
	 * @param[in] ip The IP address to check.
	 * @return True if the IP address is within the network, false otherwise.
	 */
	bool Contains(const IPAddr& ip) const;

	/**
	 * @brief Converts the network to its string representation.
	 * @return String representation of the network in CIDR notation.
	 */
	std::string ToString() const;

private:
	IPAddr _ip; ///< The base IP address of the network.
	uint64_t _mask0; ///< The first 64 bits of the network mask.
	uint64_t _mask1; ///< The second 64 bits of the network mask.
	uint8_t _prefixLen; ///< The prefix length of the network.
};

} // namespace ft::analyzer
