/**
 * @file
 * @author Lukáš Huták <hutak@cesnet.cz>
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief IP address and network representation and manipulation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ipaddr.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <arpa/inet.h>
#include <endian.h>

namespace ft::analyzer {

IPAddr::IPAddr()
{
	u64.fill(0);
}

IPAddr::IPAddr(const std::string& ip)
{
	uint32_t ipv4Be;

	if (inet_pton(AF_INET, ip.c_str(), &ipv4Be) == 1) {
		u64[0] = 0;
		u32[2] = htobe32(0x0000FFFF);
		u32[3] = ipv4Be;
		return;
	}

	if (inet_pton(AF_INET6, ip.c_str(), u64.data()) == 1) {
		return;
	}

	throw std::invalid_argument("Unable to convert '" + ip + "' to IP address");
}

bool IPAddr::IsIPv4() const
{
	return u64[0] == 0 && u32[2] == htobe32(0x0000FFFF);
}

bool IPAddr::IsIPv6() const
{
	return !IsIPv4();
}

std::string IPAddr::ToString() const
{
	if (IsIPv4()) {
		std::array<char, INET_ADDRSTRLEN> buffer;
		const char* result = inet_ntop(AF_INET, &u32[3], buffer.data(), buffer.size());
		if (!result) {
			throw std::runtime_error("Failed to convert IPv4 to string");
		}

		return std::string(buffer.data());
	} else {
		std::array<char, INET6_ADDRSTRLEN> buffer;
		const char* result = inet_ntop(AF_INET6, u8.data(), buffer.data(), buffer.size());
		if (!result) {
			throw std::runtime_error("Failed to convert IPv6 to string");
		}

		return std::string(buffer.data());
	}
}

bool IPAddr::operator==(const IPAddr& other) const
{
	return std::memcmp(u64.data(), other.u64.data(), 16) == 0;
}

bool IPAddr::operator!=(const IPAddr& other) const
{
	return !(*this == other);
}

bool IPAddr::operator<(const IPAddr& other) const
{
	return std::memcmp(u64.data(), other.u64.data(), 16) < 0;
}

bool IPAddr::operator<=(const IPAddr& other) const
{
	return !(*this > other);
}

bool IPAddr::operator>(const IPAddr& other) const
{
	return std::memcmp(u64.data(), other.u64.data(), 16) > 0;
}

bool IPAddr::operator>=(const IPAddr& other) const
{
	return !(*this < other);
}

constexpr uint64_t U64_ONES = std::numeric_limits<uint64_t>::max();

static inline uint64_t SafeShiftR(uint64_t val, unsigned count)
{
	if (count >= 64) {
		return 0;
	} else {
		return val >> count;
	}
}

IPNetwork::IPNetwork(const std::string& ip, uint8_t prefixLen)
	: _ip(ip)
	, _prefixLen(prefixLen)
{
	if (_ip.IsIPv4()) {
		if (prefixLen > 32) {
			throw std::invalid_argument(
				"Prefix length for the IPv4 network must be less or equal to 32.");
		}
		_mask0 = U64_ONES;
		_mask1 = U64_ONES >> (32 - prefixLen);
	} else {
		if (prefixLen > 128) {
			throw std::invalid_argument(
				"Prefix length for the IPv6 network must be less or equal to 128.");
		}
		_mask0 = SafeShiftR(U64_ONES, std::max(64 - prefixLen, 0));
		_mask1 = SafeShiftR(U64_ONES, 64 - std::max(prefixLen - 64, 0));
	}
}

IPNetwork::IPNetwork(const std::string& ip)
	: _ip(ip)
{
	if (_ip.IsIPv4()) {
		_prefixLen = 32;
	} else {
		_prefixLen = 128;
	}
	_mask0 = U64_ONES;
	_mask1 = U64_ONES;
}

bool IPNetwork::Contains(const IPAddr& ip) const
{
	return (ip.u64[0] & _mask0) == _ip.u64[0] && (ip.u64[1] & _mask1) == _ip.u64[1];
}

std::string IPNetwork::ToString() const
{
	return _ip.ToString() + "/" + std::to_string(_prefixLen);
}

} // namespace ft::analyzer
