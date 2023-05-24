/**
 * @file
 * @brief UDP layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief UDP header with auxiliary functions.
 */
struct UDP {
	uint16_t _srcPort;
	uint16_t _dstPort;
	uint16_t _length; /**< Total length including header and payload */
	uint16_t _checksum;

	/** @brief Exact size of the header */
	static constexpr uint16_t HEADER_SIZE = 8;
	/** @brief Minimum value of Length */
	static constexpr uint16_t LENGTH_MIN_VALUE = 8U;

	/** @brief Check if the length definition is valid. */
	bool IsValid() const noexcept { return be16toh(_length) >= LENGTH_MIN_VALUE; }
} __attribute__((packed));

static_assert(sizeof(UDP) == UDP::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
