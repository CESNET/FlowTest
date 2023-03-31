/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Builder implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetBuilder.hpp"

#include <cstring>
#include <memory>

#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

namespace replay {

struct VlanHeader {
	uint16_t vlanTci;
	uint16_t etherType;
} __attribute__((packed));

void PacketBuilder::SetVlan(uint16_t vlanID)
{
	_vlanID = vlanID;
}

void PacketBuilder::SetTimeMultiplier(float timeMultiplier)
{
	_timeMultiplier = timeMultiplier;
}

std::unique_ptr<Packet> PacketBuilder::Build(const RawPacket* rawPacket)
{
	Packet packet;
	packet.timestamp = GetMultipliedTimestamp(rawPacket->timestamp);
	packet.dataLen = rawPacket->dataLen;
	packet.info = GetPacketL3Info(rawPacket);
	if (_vlanID) {
		packet.info.l3Offset += sizeof(VlanHeader);
		packet.dataLen += sizeof(VlanHeader);
		packet.data = GetDataCopyWithVlan(rawPacket->data, rawPacket->dataLen);
	} else {
		packet.data = GetDataCopy(rawPacket->data, rawPacket->dataLen);
	}
	return std::make_unique<Packet>(std::move(packet));
}

uint64_t PacketBuilder::GetMultipliedTimestamp(uint64_t rawPacketTimestamp) const
{
	return rawPacketTimestamp * _timeMultiplier;
}

PacketInfo PacketBuilder::GetPacketL3Info(const RawPacket* rawPacket) const
{
	CheckSufficientDataLength(rawPacket->dataLen, sizeof(ethhdr));

	PacketInfo info;
	const ethhdr* ethHeader = reinterpret_cast<const ethhdr*>(rawPacket->data);
	switch (ntohs(ethHeader->h_proto)) {
	case ETH_P_IP:
		CheckSufficientDataLength(rawPacket->dataLen, sizeof(ethhdr) + sizeof(iphdr));
		info.l3Type = L3Type::IPv4;
		break;
	case ETH_P_IPV6:
		CheckSufficientDataLength(rawPacket->dataLen, sizeof(ethhdr) + sizeof(ip6_hdr));
		info.l3Type = L3Type::IPv6;
		break;
	default:
		_logger->error("Cannot parse L3 header, unsuported protocol");
		throw std::runtime_error("PacketBuilder::GetPacketL3Info() has failed");
	}
	info.l3Offset = sizeof(ethhdr);
	return info;
}

void PacketBuilder::CheckSufficientDataLength(size_t availableLength, size_t requiredLength) const
{
	if (availableLength < requiredLength) {
		_logger->error("Unsufficient packet data length");
		throw std::runtime_error("PacketBuilder::CheckSufficientDataLength() has failed");
	}
}

std::unique_ptr<std::byte[]> PacketBuilder::GetDataCopy(const std::byte* rawData, uint16_t dataLen)
{
	std::unique_ptr<std::byte[]> packetData;
	packetData = std::unique_ptr<std::byte[]>(new std::byte[dataLen]);
	std::memcpy(packetData.get(), rawData, dataLen);
	return packetData;
}

std::unique_ptr<std::byte[]>
PacketBuilder::GetDataCopyWithVlan(const std::byte* rawData, uint16_t dataLen)
{
	std::unique_ptr<std::byte[]> packetData;
	packetData = std::unique_ptr<std::byte[]>(new std::byte[dataLen + sizeof(VlanHeader)]);
	std::memcpy(packetData.get(), rawData, sizeof(ethhdr));
	std::memcpy(
		packetData.get() + sizeof(ethhdr) + sizeof(VlanHeader),
		rawData + sizeof(ethhdr),
		dataLen - sizeof(ethhdr));

	constexpr uint16_t vlanEtherType = 0x8100;

	ethhdr* ethHeader = reinterpret_cast<ethhdr*>(packetData.get());
	uint16_t originL2EtherType = ethHeader->h_proto;
	ethHeader->h_proto = htons(vlanEtherType);

	VlanHeader* vlanHeader = reinterpret_cast<VlanHeader*>(packetData.get() + sizeof(ethhdr));
	vlanHeader->vlanTci = htons(_vlanID);
	vlanHeader->etherType = originL2EtherType;

	return packetData;
}

} // namespace replay
