/**
 * @file
 * @brief IPv4 layer
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
 * @brief IPv4 header with auxiliary functions.
 */
struct IPv4 {
	uint8_t _versionIhl; /**< IP version and IHL value */
	uint8_t _typeOfService;
	uint16_t _totalLength; /**< Total length including header and data */
	uint16_t _packetId;
	uint16_t _fragmentOffset; /**< Fragmentation flags and offset */
	uint8_t _timeToLive;
	uint8_t _nextProtoId;
	uint16_t _hdrChecksum;
	uint32_t _srcAddr;
	uint32_t _dstAddr;
	// ... data ...

	/** @brief IPv4 version */
	static constexpr uint8_t VERSION = 4;
	/** @brief Minimum size of IPv4 header */
	static constexpr uint16_t HEADER_SIZE_MIN = 20;
	/** @brief Maximum size of IPv4 header */
	static constexpr uint16_t HEADER_SIZE_MAX = 60;
	/** @brief Minimum value of IHL (Internet Header Length) */
	static constexpr uint8_t IHL_MIN = 5;
	/** @brief Relative position of More Fragment flag */
	static constexpr int FRAGMENT_MF_SHIFT = 13;
	/** @brief Relative position of Dont Fragment flag */
	static constexpr int FRAGMENT_DF_SHIFT = 14;
	/** @brief Bitmask of Fragment Offset field */
	static constexpr uint16_t FRAGMENT_OFFSET_MASK = 0x07FF;

	/** @brief Check if the version and length definition is valid. */
	bool IsValid() const noexcept { return GetVersion() == VERSION && GetIhl() >= IHL_MIN; };

	/** @brief Get IP version field. */
	uint8_t GetVersion() const noexcept { return (_versionIhl >> 4U) & 0x0F; }

	/**
	 * @brief Get IHL (Internet Header Length) field.
	 * @note This value does NOT represent the real header size.
	 */
	uint8_t GetIhl() const noexcept { return (_versionIhl & 0x0F); }

	/** @brief Get the real header length including optional extensions. */
	uint16_t GetHdrLength() const noexcept { return GetIhl() * 4U; };

	/**
	 * @brief Get fragment offset.
	 * @note This value represents the real offset (i.e. the header value multiplied by 8).
	 */
	uint16_t GetFragmentOffset() const noexcept
	{
		return (be16toh(_fragmentOffset) & FRAGMENT_OFFSET_MASK) * 8U;
	}

	/** @brief Determine type of the fragment. */
	IPFragmentType GetFragmentType() const noexcept
	{
		const bool mfSet = (be16toh(_fragmentOffset) & (1U << FRAGMENT_MF_SHIFT)) != 0;
		const uint16_t offset = GetFragmentOffset();

		if (!offset) {
			return (!mfSet) ? IPFragmentType::None : IPFragmentType::First;
		} else {
			return (!mfSet) ? IPFragmentType::Last : IPFragmentType::Middle;
		}
	}
} __attribute__((packed));

static_assert(sizeof(IPv4) == IPv4::HEADER_SIZE_MIN, "Unexpected header size");

} // namespace replay::protocol
