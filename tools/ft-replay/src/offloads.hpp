/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definitions for managing network offloading flags.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config.hpp"

namespace replay {

/**
 * @brief Structure representing network checksum offloading options.
 */
struct ChecksumOffloads {
	bool checksumIPv4; ///< Flag for IPv4 checksum offload support.
	bool checksumTCP; ///< Flag for TCP checksum offload support.
	bool checksumUDP; ///< Flag for UDP checksum offload support.
	bool checksumICMPv6; ///< Flag for ICMPv6 checksum offload support.
};

/**
 * @brief Structure representing network offload requests.
 */
struct OffloadRequests {
	ChecksumOffloads checksumOffloads; ///< Checksum offload requests.
	Config::RateLimit rateLimit; ///< Rate limiting configuration.
};

/**
 * @brief Enumeration representing network offloading flags.
 *
 * This enumeration defines various offloading features that can be supported.
 */
enum class Offload {
	CHECKSUM_IPV4 = 0x1, ///< Support IPv4 checksum offload.
	CHECKSUM_UDP = 0x2, ///< Support UDP checksum offload.
	CHECKSUM_TCP = 0x4, ///< Support TCP checksum offload.
	CHECKSUM_ICMPV6 = 0x8, ///< Support ICMPv6 checksum offload.
	RATE_LIMIT_PACKETS = 0x10, ///< Support rate limiting based on packets.
	RATE_LIMIT_BYTES = 0x20, ///< Support rate limiting based on bytes.
	RATE_LIMIT_TIME = 0x30 ///< Support rate limiting based on time intervals.
};

/**
 * @typedef Offloads
 * @brief Type alias for a combination of network offloading flags.
 *
 * This type alias represents a combination of network offloading flags stored as a uint64_t.
 */
using Offloads = uint64_t;

inline Offloads operator|(Offload lhs, Offload rhs)
{
	return static_cast<Offloads>(lhs) | static_cast<Offloads>(rhs);
}

inline bool operator&(Offloads offloads, Offload offload)
{
	return (static_cast<Offloads>(offload) & offloads) != 0;
}

inline Offloads& operator|=(Offloads& offloads, Offload offload)
{
	offloads |= static_cast<Offloads>(offload);
	return offloads;
}

} // namespace replay
