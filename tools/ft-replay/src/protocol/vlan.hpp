/**
 * @file
 * @brief VLAN layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::protocol {

/**
 * @brief IEEE 802.1Q VLAN.
 * @note
 *  Compared to the usual definition, the structure is modified to
 *  contain only the VLAN ID and the following Ethernet type, i.e.
 *  it does not contain its own identifier at the beginning.
 */
struct VLAN {
	uint16_t _VlanId; /**< Flags and VLAN ID */
	uint16_t _Ethertype; /**< Next header type */

	/** @brief Exact size of the header */
	static constexpr uint16_t HEADER_SIZE = 4;
} __attribute__((packed));

static_assert(sizeof(VLAN) == VLAN::HEADER_SIZE, "Unexpected header size");

} // namespace replay::protocol
