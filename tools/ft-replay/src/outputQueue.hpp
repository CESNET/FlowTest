/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>
#include <vector>

namespace replay {

/**
 * @brief Basic packet buffer info
 *
 */
struct PacketBuffer {
	std::byte* _data;
	size_t _len;
};

/**
 * @brief Output queue virtual interface
 *
 * First, the user must fill array of PacketBuffer structure with required packets length
 * and than call function GetBurst that assign memory of requiered length to PacketBuffer
 * _data variable.
 * Assigned memory has to be filled by user after calling GetBurst function with valid
 * packet data.
 *
 * Function SendBurst has to be called immediately after GetBurst.
 * It is not possible to call multiple GetBurst without calling SendBurst,
 * an unexpected behaviour may occur
 */
struct OutputQueue {

	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return maximal possible burst size
	 */
	virtual size_t GetMaxBurstSize() const noexcept = 0;

	/**
	 * @brief Get up to @p burstSize PacketBuffers
	 *
	 * Function expects filled variable @p _len with requiered packet length
	 * in PacketBuffer structure array.
	 * Function fill the @p _data pointers with memory that can hold @p _len bytes.
	 *
	 * After this function the user must fill the @p _data memory with valid packet data.
	 * Function may return less buffers than required @p burstSize
	 *
	 * @param[in,out] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of PacketBuffers
	 *
	 * @return number of assigned buffers
	 */
	virtual size_t GetBurst(PacketBuffer* burst, size_t burstSize) = 0;

	/**
	 * @brief Send burst of packets
	 *
	 * @warning The parameter @p burstSize must have the same value as the return
	 *          value of the function GetBurst, otherwise unexpected behavior may occur.
	 *
	 * @param[in] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of packets to send in burst
	 */
	virtual void SendBurst(const PacketBuffer* burst, size_t burstSize) = 0;

};

} // namespace replay
