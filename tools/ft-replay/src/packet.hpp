/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Packet interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>

namespace replay {

/**
 * @brief Enum representing the packet IP type
 *
 */
enum class L3Type {
	IPv4,
	IPv6,
};

/**
 * @brief Enum representing the transport layer for checksum offloading
 */
enum class L4Type {
	TCP,
	UDP,
	ICMPv6,
	Other,
	NotFound,
};

/**
 * @brief Enum representing the output interface when replaying in multi-port mode
 */
enum class OutInterface {
	Interface0,
	Interface1,
};

/**
 * @brief Class representing the packet L3 info
 *
 */
struct PacketInfo {
	enum L3Type l3Type;
	uint16_t l3Offset;

	enum L4Type l4Type;
	uint16_t l4Offset; //< Zero if l4type == L4Type::NotFound

	uint16_t ipAddressesChecksum; //< checksum of IP addresses in host byte order

	enum OutInterface outInterface; //< used for multi-port replaying
};

/**
 * @brief Struct representing packet data.
 */
struct Packet {
	/** @brief Packet data. */
	std::unique_ptr<std::byte[]> data;
	/** @brief Packet data length. */
	uint16_t dataLen;
	/** @brief Packet timestamp. */
	uint64_t timestamp;
	/** @brief Packet L3 info. */
	PacketInfo info;
};

/**
 * @brief Class for calculating hash value of packet based on its IP version.
 */
class PacketHashCalculator {
public:
	PacketHashCalculator() = default;

	/**
	 * @brief Get the Packet hash value
	 */
	uint32_t GetHash(const Packet& packet) const;

private:
	uint32_t CalculateIpv4Hash(const Packet& packet) const;
	uint32_t CalculateIpv6Hash(const Packet& packet) const;
};

} // namespace replay

namespace std {

template <>
struct hash<replay::Packet> {
	size_t operator()(const replay::Packet& packet) const
	{
		replay::PacketHashCalculator hashCalculator;
		return hashCalculator.GetHash(packet);
	}
};

} // namespace std
