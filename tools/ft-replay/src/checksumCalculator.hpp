/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the declaration of the CalculateIPAddressesChecksum function.
 */

#pragma once

#include "packet.hpp"

#include <cstddef>
#include <cstdint>

namespace replay {

/**
 * @brief Calculate the checksum for the source and destination IP addresses in an IP packet header.
 *
 * This function calculates the checksum for the source and destination IP addresses in an IP packet
 * header. It supports both IPv4 and IPv6 headers.
 *
 * @param ptr A pointer to the memory buffer containing the IP packet data.
 * @param l3Type An enumeration indicating the type of Layer 3 (L3) protocol, which can be either
 * L3Type::IPv4 or L3Type::IPv6.
 * @param l3Offset An offset that points to the beginning of the Layer 3 header within the packet.
 * @return The calculated IP addresses checksum as a 16-bit unsigned integer.
 */
uint16_t CalculateIPAddressesChecksum(const std::byte* ptr, enum L3Type l3Type, uint16_t l3Offset);

/**
 * @brief Calculate the IPv4/IPv6 pseudo header checksum for the given packet.
 *
 * This function calculates the pseudo header checksum for a packet using the pseudo header
 * information contained in the packet structure.
 *
 * @param packet The packet for which to calculate the pseudo header checksum.
 * @return The calculated pseudo header checksum as a 16-bit unsigned integer.
 */
uint16_t CalculatePsedoHeaderChecksum(const Packet& packet);

} // namespace replay
