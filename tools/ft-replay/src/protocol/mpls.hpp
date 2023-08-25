/**
 * @file
 * @brief MPLS layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief MPLS header.
 *
 * @note
 *   Multiple MPLS headers can be stacked in a row. Use BoS flag to
 *   determine the last header.
 *
 *   The header doesn't contain information about the protocol
 *   (after the last MPLS label). It is usually IPv4/IPv6 traffic
 *   and it must be manually distinguished based on the first nibble
 *   right after the last MPLS header.
 */
struct MPLS {
	uint32_t values; // Label, Traffic Class, Flags, TTL

	/** @brief Exact size of the header */
	static constexpr uint16_t HEADER_SIZE = 4;
	/** @brief Relative position of BoS flag */
	static constexpr int FLAG_BOS_SHIFT = 8;

	/**
	 * @brief Test whether Bottom of the Stack flag is set.
	 *
	 * If this is set, it signifies that the current label is the last
	 * in the stack.
	 */
	bool IsBosSet() const noexcept { return (be32toh(values) & (1U << FLAG_BOS_SHIFT)) != 0; }
} __attribute__((packed));

static_assert(sizeof(MPLS) == MPLS::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
