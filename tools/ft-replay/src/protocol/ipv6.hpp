/**
 * @file
 * @brief IPv6 layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "ipFragmentType.hpp"

#include <array>
#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief IPv6 header with auxiliary functions
 */
struct IPv6 {
	uint32_t _vtcFlow; /**< IP version, traffic class and flow label */
	uint16_t _payloadLength; /**< Payload size including extension headers */
	uint8_t _nextProtoId;
	uint8_t _hopLimit;
	std::array<std::byte, 16> _srcAddr;
	std::array<std::byte, 16> _dstAddr;

	/** @brief IPv6 version */
	static constexpr uint8_t VERSION = 6;
	/** @brief Exact size of IPv6 header */
	static constexpr uint16_t HEADER_SIZE = 40;
	/** @brief Relative position of Version field */
	static constexpr int VERSION_SHIFT = 28;
	/** @brief Bitmask of Version field */
	static constexpr uint16_t VERSION_MASK = 0x0F;

	/** @brief Check if the version definition is valid. */
	bool IsValid() const noexcept { return GetVersion() == VERSION; }

	/** @brief Get IP version field. */
	uint8_t GetVersion() const noexcept
	{
		return (be32toh(_vtcFlow) >> VERSION_SHIFT) & VERSION_MASK;
	}
} __attribute__((packed));

static_assert(sizeof(IPv6) == IPv6::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
