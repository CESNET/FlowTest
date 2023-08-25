/**
 * @file
 * @brief TCP layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <endian.h>

namespace replay::protocol {

/**
 * @brief TCP header with auxiliary functions.
 */
struct TCP {
	uint16_t _srcPort;
	uint16_t _dstPort;
	uint32_t _sentSeq;
	uint32_t _recvAck;
	uint8_t _dataOffset; /**< Data offset, reserved and NS flag */
	uint8_t _tcpFlags;
	uint16_t _windowSize;
	uint16_t _checksum;
	uint16_t _tcpUrgentPtr;
	// ... data ...

	/** @brief Minimum size of the header to be casted and interpreted. */
	static constexpr uint16_t HEADER_SIZE_MIN = 20;
	/** @brief Minimum value of Data Offset field */
	static constexpr uint8_t DATA_OFFSET_MIN = 5;
	/** @brief Maximum value of Data Offset field */
	static constexpr uint8_t DATA_OFFSET_MAX = 15;
	/** @brief Relative position of Data Offset field */
	static constexpr int DATA_OFFSET_SHIFT = 4;
	/** @brief Bitmask of Data Offset field */
	static constexpr uint8_t DATA_OFFSET_MASK = 0x0F;

	/** @brief Check if the length definition is valid. */
	bool IsValid() const noexcept
	{
		const uint8_t dataOffset = GetDataOffset();
		return dataOffset >= DATA_OFFSET_MIN && dataOffset <= DATA_OFFSET_MAX;
	};

	/** @brief Get the real header length. */
	uint16_t GetHdrLength() const noexcept { return GetDataOffset() * 4U; }

	/** @brief Get the value of Data Offset field */
	uint8_t GetDataOffset() const noexcept
	{
		return (_dataOffset >> DATA_OFFSET_SHIFT) & DATA_OFFSET_MASK;
	}
} __attribute__((packed));

static_assert(sizeof(TCP) == TCP::HEADER_SIZE_MIN, "Unexpected header size");

} // namespace replay::protocol
