/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Builder interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "packet.hpp"
#include "rawPacketProvider.hpp"

#include <memory>

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
	 * @brief Set the time multiplier value
	 *
	 * Timestamp in packet are multiplied by this value
	 * Default: 0
	 *
	 * @param timeMultiplier Value to set
	 */
	void SetTimeMultiplier(float timeMultiplier);

private:
	uint64_t GetMultipliedTimestamp(uint64_t rawPacketTimestamp) const;
	PacketInfo GetPacketL3Info(const RawPacket* rawPacket) const;
	void CheckSufficientDataLength(size_t availableLength, size_t requiredLength) const;
	std::unique_ptr<std::byte[]> GetDataCopy(const std::byte* rawData, uint16_t dataLen);
	std::unique_ptr<std::byte[]> GetDataCopyWithVlan(const std::byte* rawData, uint16_t dataLen);

	uint16_t _vlanID = 0;
	float _timeMultiplier = 0;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PacketBuilder");
};

} // namespace replay
