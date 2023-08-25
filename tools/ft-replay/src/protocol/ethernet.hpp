/**
 * @file
 * @brief Ethernet layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief Ethernet header.
 */
struct Ethernet {
	std::array<std::byte, 6> _srcMac;
	std::array<std::byte, 6> _dstMac;
	uint16_t _ethertype; /**< Next header type */

	static constexpr uint16_t HEADER_SIZE = 14U;
	/** @brief Minimum valid value of ethertype */
	static constexpr uint16_t ETHERTYPE_MIN = 0x0600;

	/**
	 * @brief Check if the EtherType has a valid value.
	 * @note
	 *   Based on https://datatracker.ietf.org/doc/html/rfc5342#section-3:
	 *   Ethertypes: These are 16-bit identifiers appearing as the initial
	 *   two octets after the MAC destination and source (or after a
	 *   tag) which, when considered as an unsigned integer, are equal
	 *   to or larger than 0x0600.
	 */
	bool IsValid() const noexcept { return be16toh(_ethertype) >= ETHERTYPE_MIN; }
} __attribute__((packed));

static_assert(sizeof(Ethernet) == Ethernet::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
