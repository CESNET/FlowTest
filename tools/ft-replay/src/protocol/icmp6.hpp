/**
 * @file
 * @brief ICMPv6 layer
 * @author Pavel Siska <siska@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::protocol {

/**
 * @brief ICMPv6 header.
 */
struct ICMPv6 {
	uint8_t _type;
	uint8_t _code;
	uint16_t _checksum;
	// ... data ...

	/** @brief Exact size of ICMPv6 header */
	static constexpr uint16_t HEADER_SIZE = 4;

} __attribute__((packed));

static_assert(sizeof(ICMPv6) == ICMPv6::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
