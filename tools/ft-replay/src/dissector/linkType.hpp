/*
 * @file
 * @brief Link type
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::dissector {

/**
 * @brief Auxiliary link type specific for PCAP capture.
 *
 * The list contains only relevant layers of non-zero length.
 * @see https://www.tcpdump.org/linktypes.html
 */
enum class LinkType : uint8_t {
	Ethernet = 1, /**< Standard Ethernet layer */
};

} // namespace replay::dissector
