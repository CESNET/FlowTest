/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Builder interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "offloads.hpp"
#include "packet.hpp"
#include "rawPacketProvider.hpp"
#include "replicator-core/macAddress.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace replay {

/**
 * @brief Packet class builder
 */
class PacketBuilder {
public:
	/**
	 * @brief Build Packet structure from RawPacket structure
	 *
	 * @param rawPacket RawPacket information
	 * @return std::unique_ptr<Packet> Builded packet
	 */
	std::unique_ptr<Packet> Build(const RawPacket* rawPacket);

	/**
	 * @brief Set the Vlan ID value
	 *
	 * If Vlan ID is set then vlan header is inserted into the packet
	 * Default: 0 - do not insert vlan header
	 *
	 * @param vlanID Vlan ID to set.
	 */
	void SetVlan(uint16_t vlanID);

	/**
	 * @brief Set the source MAC address
	 *
	 * If the MAC address is unspecified, the original source MAC address is preserved.
	 * Default: std::nullopt - do not modify the address
	 *
	 * @param address MAC address to set.
	 */
	void SetSrcMac(const std::optional<MacAddress>& address);

	/**
	 * @brief Set the destination MAC address
	 *
	 * If the MAC address is unspecified, the original destination MAC address is preserved.
	 * Default: std::nullopt - do not modify the address
	 *
	 * @param address MAC address to set.
	 */
	void SetDstMac(const std::optional<MacAddress>& address);

	/**
	 * @brief Set the Time Multiplier value
	 *
	 * Packet timestamp is multiplied by this value.
	 */
	void SetTimeMultiplier(double timeMultiplier);

	/**
	 * @brief Set hardware offloads configuration for checksum preset.
	 *
	 * This method allows you to set the hardware offloads configuration for checksum preset
	 * based on the specified `hwChecksumOffloads`.
	 *
	 * @param hwChecksumOffloads The hardware offloads configuration for checksum preset.
	 */
	void SetHwOffloads(const Offloads hwChecksumOffloads);

	/**
	 * @brief Set the Maximum Transmission Unit (MTU) size.
	 *
	 * Built packets cannot exceed this size.
	 * If packet is larger than MTU, exception is thrown.
	 *
	 * @param mtu Maximum Transmission Unit (MTU) size to set.
	 */
	void SetMTU(uint16_t mtu);

private:
	PacketInfo GetPacketInfo(const RawPacket* rawPacket) const;
	std::unique_ptr<std::byte[]> GetDataCopy(const std::byte* rawData, uint16_t dataLen);
	std::unique_ptr<std::byte[]> GetDataCopyWithVlan(const std::byte* rawData, uint16_t dataLen);
	void ValidatePacketLength(uint16_t packetLength) const;
	void PresetHwChecksum(Packet& packet);
	int DecidePort(const std::byte* rawData, enum L3Type l3Type, uint16_t l3Offset) const;

	uint16_t _vlanID = 0;
	double _timeMultiplier = 1.0;
	uint16_t _mtu = std::numeric_limits<uint16_t>::max();
	Offloads _hwOffloads = 0;
	std::optional<MacAddress> _srcMac = std::nullopt;
	std::optional<MacAddress> _dstMac = std::nullopt;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PacketBuilder");
};

} // namespace replay
