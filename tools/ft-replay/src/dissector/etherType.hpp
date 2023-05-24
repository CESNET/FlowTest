/*
 * @file
 * @brief Ethernet type
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::dissector {

/**
 * @brief EtherType values.
 * @see https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml
 */
enum class EtherType : uint16_t {
	IPv4 = 0x0800,
	VLAN = 0x8100, /**< IEEE Std 802.1Q - Customer VLAN Tag Type (C-Tag) */
	IPv6 = 0x86DD,
	MPLS = 0x8847,
	MPLSUpstream = 0x8848,
	VLANSTag = 0x88A8, /**< IEEE Std 802.1Q - Service VLAN tag identifier (S-Tag) */
};

} // namespace replay::dissector
