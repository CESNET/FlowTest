/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>

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
 * First, the user must fill array of PacketBuffer structure with required
 * packets length and than call function GetBurst that assign memory of
 * requiered length to PacketBuffer _data variable. Assigned memory has to be
 * filled by user after calling GetBurst function with valid packet data.
 *
 * Function SendBurst has to be called immediately after GetBurst.
 * It is not possible to call multiple GetBurst without calling SendBurst,
 * an unexpected behaviour may occur
 */
class OutputQueue {
public:
	/**
	 * @brief Get the Maximal Burst Size
	 *
	 * @return maximal possible burst size
	 */
	virtual size_t GetMaxBurstSize() const noexcept = 0;

	/**
	 * @brief Get @p burstSize PacketBuffers
	 *
	 * Function expects filled variable @p _len with requiered packet length
	 * in PacketBuffer structure array.
	 * Function fill the @p _data pointers with memory that can hold @p _len
	 * bytes.
	 *
	 * After this function the user must fill the @p _data memory with valid
	 * packet data.
	 *
	 * @warning @p burstSize cannot be bigger than GetMaxBurstSize()
	 *
	 * @param[in,out] burst pointer to PacketBuffer array
	 * @param[in] burstSize number of PacketBuffers
	 */
	virtual void GetBurst(PacketBuffer* burst, size_t burstSize) = 0;

	/**
	 * @brief Send burst of packets
	 *
	 * @param[in] burst pointer to PacketBuffer array
	 */
	virtual void SendBurst(const PacketBuffer* burst) = 0;

	/**
	 * @brief Default virtual destructor
	 */
	virtual ~OutputQueue() = default;
};

} // namespace replay
