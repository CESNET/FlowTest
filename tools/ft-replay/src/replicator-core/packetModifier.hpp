/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief packet modifier
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../packet.hpp"
#include "strategy.hpp"

namespace replay {

/**
 * @brief Class holds strategies how to modify given packet
 */
class PacketModifier {
public:
	/**
	 * @brief Construct a new Packet Modifier object
	 *
	 * @param modifierStrategies Strategies how to modify the packet
	 */
	PacketModifier(ModifierStrategies modifierStrategies);

	/**
	 * @brief Checks if a packet modifier is in specific loop enabled.
	 *
	 * This function checks whether a loop with the given ID is enabled or not.
	 *
	 * @param loopId The ID of the loop to check.
	 * @return True if the loop is enabled, false otherwise.
	 */
	bool IsEnabledThisLoop(size_t loopId) noexcept;

	/**
	 * @brief Apply packet modifier strategies to given packet
	 *
	 * @param ptr Pointer to the beggining of packet data
	 * @param pktInfo Information about given packet pointer @p ptr
	 * @param loopId Current replication loop ID
	 */
	void Modify(std::byte* ptr, const PacketInfo& pktInfo, size_t loopId);

private:
	ModifierStrategies _strategy;
};

} // namespace replay
