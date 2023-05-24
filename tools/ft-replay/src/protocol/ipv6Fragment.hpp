/**
 * @file
 * @brief IPv6 Fragment layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "ipFragmentType.hpp"

#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief IPv6 Fragment header.
 */
struct IPv6Fragment {
	uint8_t _nextProtoId;
	uint8_t _reserved;
	uint16_t _fragmentOffset; /**< Fragmentation flags and offset */
	uint32_t _identification;

	/** @brief Exact size of the header */
	static constexpr uint16_t HEADER_SIZE = 8;
	/** @brief Relative position of Fragment Offset field */
	static constexpr int FRAGMENT_OFFSET_SHIFT = 3;
	/** @brief Bitmask of Fragment Offset field */
	static constexpr uint16_t FRAGMENT_OFFSET_MASK = 0x1FFF;

	/**
	 * @brief Get fragment offset.
	 * @note This value represents the real offset (i.e. the header value multiplied by 8).
	 */
	uint16_t GetFragmentOffset() const noexcept
	{
		uint16_t tmp = be16toh(_fragmentOffset) >> FRAGMENT_OFFSET_SHIFT;
		return (tmp & FRAGMENT_OFFSET_MASK) * 8U;
	}

	/** @brief Determine type of the fragment */
	IPFragmentType GetFragmentType() const noexcept
	{
		const bool mfSet = (be16toh(_fragmentOffset) & 0x01) != 0;
		const uint16_t offset = GetFragmentOffset();

		if (!offset) {
			return (!mfSet) ? IPFragmentType::None : IPFragmentType::First;
		} else {
			return (!mfSet) ? IPFragmentType::Last : IPFragmentType::Middle;
		}
	}
} __attribute__((packed));

static_assert(sizeof(IPv6Fragment) == IPv6Fragment::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
