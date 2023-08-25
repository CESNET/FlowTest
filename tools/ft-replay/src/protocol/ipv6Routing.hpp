/**
 * @file
 * @brief IPv6 Routing layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::protocol {

/**
 * @brief IPv6 Routing header.
 */
struct IPv6Routing {
	uint8_t _nextProtoId;
	uint8_t _extensionLen; /**< Not including first 8 octets */
	uint8_t _routingType;
	uint8_t _segmentsLeft;
	// ... data ...

	/** @brief Minimum size of the header to be casted and interpreted. */
	static constexpr uint16_t HEADER_SIZE_MIN = 4;

	/** @brief Get the real length of the header */
	uint16_t GetHdrLength() const noexcept { return (_extensionLen + 1U) * 8U; }
} __attribute__((packed));

static_assert(sizeof(IPv6Routing) == IPv6Routing::HEADER_SIZE_MIN, "Unexpected header size");

} // namespace replay::protocol
