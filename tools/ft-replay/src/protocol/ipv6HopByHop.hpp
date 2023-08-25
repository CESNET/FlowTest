/**
 * @file
 * @brief IPv6 Hop-by-Hop layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::protocol {

/**
 * @brief IPv6 Hop-by-Hop header.
 */
struct IPv6HopByHop {
	uint8_t _nextProtoId;
	uint8_t _extensionLen; /**< Not including first 8 octets */
	// ... data ...

	/** @brief Minimum size of the header to be casted and interpreted. */
	static constexpr uint16_t HEADER_SIZE_MIN = 2;

	/** @brief Get the real length of the header */
	uint16_t GetHdrLength() const noexcept { return (_extensionLen + 1U) * 8U; }
} __attribute__((packed));

static_assert(sizeof(IPv6HopByHop) == IPv6HopByHop::HEADER_SIZE_MIN, "Unexpected header size");

} // namespace replay::protocol
