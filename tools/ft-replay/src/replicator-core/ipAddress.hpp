/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator Ip address
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <variant>

#include <arpa/inet.h>

namespace replay {

/**
 * @brief Represent basic operation over IP address pointer
 */
class IpAddressView {
public:
	using Ip4Ptr = uint32_t*;
	using Ip6Ptr = __int128_t*;

	IpAddressView(Ip4Ptr ip4Ptr)
		: _ipPtr(ip4Ptr)
	{
	}

	IpAddressView(Ip6Ptr ip6Ptr)
		: _ipPtr(ip6Ptr)
	{
	}

	IpAddressView operator+=(uint32_t value)
	{
		if (IsIp4()) {
			Ip4Ptr ip = std::get<Ip4Ptr>(_ipPtr);
			*ip = htonl(ntohl(*ip) + value);
		} else {
			Ip6Ptr ip = std::get<Ip6Ptr>(_ipPtr);
			uint32_t* ipAs32b = reinterpret_cast<uint32_t*>(ip);
			ipAs32b[0] = htonl(ntohl(ipAs32b[0]) + value);
		}
		return *this;
	}

private:
	std::variant<Ip4Ptr, Ip6Ptr> _ipPtr;

	bool IsIp4() { return std::holds_alternative<Ip4Ptr>(_ipPtr); }
};

} // namespace replay
