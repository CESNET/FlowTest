/*
 * @file
 * @brief Packet layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "etherType.hpp"
#include "linkType.hpp"
#include "payloadType.hpp"
#include "protocolType.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace replay::dissector {

/**
 * @brief Packet layer type.
 *
 * Holds the type and an additional information (e.g. protocol number).
 */
using LayerType = std::variant<std::monostate, LinkType, EtherType, ProtocolType, PayloadType>;

/**
 * @brief Categorization of internet protocol layers.
 */
enum class LayerNumber {
	L2 = 2, // Link
	L3 = 3, // Internet
	L4 = 4, // Transport
	L7 = 7, // Application
};

/**
 * @brief Packet layer.
 *
 * The structure describe a single layer of a packet.
 */
struct Layer {
	LayerType _type; /**< Layer type */
	size_t _offset; /**< Offset from the packet beginning */
};

/**
 * @brief Convert packet layer to layer number.
 */
LayerNumber LayerType2Number(LayerType type);

} // namespace replay::dissector
