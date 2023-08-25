/*
 * @file
 * @brief Payload type
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

namespace replay::dissector {

/**
 * @brief Auxiliary identification of packet payload.
 */
enum class PayloadType : uint8_t {
	Unknown, /**< Unknown, unsupported or invalid payload */
	IPFragment, /**< Non-first fragment of IPv4/IPv6 payload */
	AppData, /**< Payload suitable for application protocol parsers */
};

} // namespace replay::dissector
