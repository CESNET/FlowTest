/*
 * @file
 * @brief Protocol type
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::dissector {

/**
 * @brief IANA Internet Protocol Number.
 * @see https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
 */
enum class ProtocolType : uint8_t {
	IPv6HopOpt = 0, /**< IPv6 Hop-by-Hop Option */
	IPv4 = 4, /**< IPv4 encapsulation (i.e. IP in IP) */
	TCP = 6, /**< Transmission Control */
	UDP = 17, /**< User Datagram */
	IPv6 = 41, /**< IPv6 encapsulation */
	IPv6Route = 43, /**< Routing Header for IPv6 */
	IPv6Frag = 44, /**< Fragment Header for IPv6 */
	IPv6NoNext = 59, /**< No Next Header for IPv6 */
	IPv6Dest = 60, /**< Destination Options for IPv6 */

	Unknown = 255, /**< Unknown/unsupported/unrecognized protocol */
};

} // namespace replay::dissector
