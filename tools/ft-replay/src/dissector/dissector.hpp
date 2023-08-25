/*
 * @file
 * @brief Packet dissector
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "layer.hpp"

#include <rawPacketProvider.hpp>

#include <stdexcept>
#include <vector>

namespace replay::dissector {

/**
 * @brief Exception of the packet dissector.
 */
class DissectorException : public std::runtime_error {
public:
	DissectorException(const std::string& whatArg)
		: std::runtime_error(whatArg) {};

	DissectorException(const char* whatArg)
		: std::runtime_error(whatArg) {};
};

/**
 * @brief Dissect packet layers.
 *
 * Parse the given @p packet and return a vector of detected layers.
 * The order of packet layers is preserved in the vector, i.e. the first
 * element of the vector corresponds to the first layer, etc.
 *
 * @param[in]  packet         Raw packet to dissect.
 * @param[in]  firstLayer     The first layer of the packet.
 * @throw DissectorException if the @p packet is malformed.
 */
std::vector<Layer> Dissect(const RawPacket& packet, LayerType firstLayer);

} // namespace replay::dissector
