/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Packet Builder implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetBuilder.hpp"

#include "checksumCalculator.hpp"
#include "dissector/dissector.hpp"

#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>

#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

namespace replay {

using namespace replay::dissector;

struct VlanHeader {
	uint16_t vlanTci;
	uint16_t etherType;
} __attribute__((packed));

void PacketBuilder::SetVlan(uint16_t vlanID)
{
	_vlanID = vlanID;
}

void PacketBuilder::SetSrcMac(const std::optional<MacAddress>& address)
{
	_srcMac = address;
}

void PacketBuilder::SetDstMac(const std::optional<MacAddress>& address)
{
	_dstMac = address;
}

void PacketBuilder::SetHwOffloads(const Offloads hwChecksumOffloads)
{
	_hwOffloads = hwChecksumOffloads;
}

void PacketBuilder::SetTimeMultiplier(double timeMultiplier)
{
	_timeMultiplier = timeMultiplier;
}

void PacketBuilder::SetMTU(uint16_t mtu)
{
	_mtu = mtu;
}

std::unique_ptr<Packet> PacketBuilder::Build(const RawPacket* rawPacket)
{
	Packet packet;
	packet.dataLen = rawPacket->dataLen;
	packet.timestamp = rawPacket->timestamp * _timeMultiplier;
	packet.info = GetPacketInfo(rawPacket);

	if (_vlanID) {
		packet.info.l3Offset += sizeof(VlanHeader);
		if (packet.info.l4Offset != 0) {
			packet.info.l4Offset += sizeof(VlanHeader);
		}
		packet.dataLen += sizeof(VlanHeader);
		packet.data = GetDataCopyWithVlan(rawPacket->data, rawPacket->dataLen);
	} else {
		packet.data = GetDataCopy(rawPacket->data, rawPacket->dataLen);
	}

	ValidatePacketLength(packet.dataLen);

	if (_srcMac) {
		ether_header* ethHeader = reinterpret_cast<ether_header*>(packet.data.get());
		MacAddress ref {reinterpret_cast<MacAddress::MacPtr>(ethHeader->ether_shost)};
		ref = _srcMac.value();
	}

	if (_dstMac) {
		ether_header* ethHeader = reinterpret_cast<ether_header*>(packet.data.get());
		MacAddress ref {reinterpret_cast<MacAddress::MacPtr>(ethHeader->ether_dhost)};
		ref = _dstMac.value();
	}

	PresetHwChecksum(packet);

	return std::make_unique<Packet>(std::move(packet));
}

void PacketBuilder::ValidatePacketLength(uint16_t packetLength) const
{
	if (packetLength > _mtu) {
		_logger->error("Packet length {} exceeds MTU {}", packetLength, _mtu);
		throw std::runtime_error("PacketBuilder::ValidatePacketLength() has failed");
	}
}

void PacketBuilder::PresetHwChecksum(Packet& packet)
{
	if (_hwOffloads & Offload::CHECKSUM_IPV4 && packet.info.l3Type == L3Type::IPv4) {
		iphdr* ipHeader = reinterpret_cast<iphdr*>(packet.data.get() + packet.info.l3Offset);
		ipHeader->check = 0;
	}
	if (_hwOffloads & Offload::CHECKSUM_UDP && packet.info.l4Type == L4Type::UDP) {
		udphdr* udpHeader = reinterpret_cast<udphdr*>(packet.data.get() + packet.info.l4Offset);
		udpHeader->check = htons(CalculatePsedoHeaderChecksum(packet));
	} else if (_hwOffloads & Offload::CHECKSUM_TCP && packet.info.l4Type == L4Type::TCP) {
		tcphdr* tcpHeader = reinterpret_cast<tcphdr*>(packet.data.get() + packet.info.l4Offset);
		tcpHeader->check = htons(CalculatePsedoHeaderChecksum(packet));
	} else if (_hwOffloads & Offload::CHECKSUM_ICMPV6 && packet.info.l4Type == L4Type::ICMPv6) {
		icmp6_hdr* icmp6Header
			= reinterpret_cast<icmp6_hdr*>(packet.data.get() + packet.info.l4Offset);
		icmp6Header->icmp6_cksum = htons(CalculatePsedoHeaderChecksum(packet));
	}
}

static std::optional<size_t> LayersFindFirstOf(
	const std::vector<dissector::Layer>& layers,
	std::initializer_list<dissector::LayerType> types,
	size_t startPos = 0)
{
	if (startPos >= layers.size()) {
		return std::nullopt;
	}

	auto layersBegin = layers.begin();
	std::advance(layersBegin, startPos);

	auto cmp = [](const dissector::Layer& layer, const dissector::LayerType& type) {
		return layer._type == type;
	};

	auto result = std::find_first_of(layersBegin, layers.end(), types.begin(), types.end(), cmp);
	if (result != layers.end()) {
		return std::distance(layers.begin(), result);
	} else {
		return std::nullopt;
	}
}

static std::optional<size_t> LayersFindFirstOf(
	const std::vector<dissector::Layer>& layers,
	LayerNumber layerNumber,
	size_t startPos = 0)
{
	if (startPos >= layers.size()) {
		return std::nullopt;
	}

	auto layersBegin = layers.begin();
	std::advance(layersBegin, startPos);

	auto cmp = [layerNumber](const dissector::Layer& layer) {
		return LayerType2Number(layer._type) == layerNumber;
	};

	auto result = std::find_if(layersBegin, layers.end(), cmp);
	if (result != layers.end()) {
		return std::distance(layers.begin(), result);
	} else {
		return std::nullopt;
	}
}

static L3Type EtherTypeToIPVersion(EtherType type)
{
	switch (type) {
	case EtherType::IPv4:
		return L3Type::IPv4;
	case EtherType::IPv6:
		return L3Type::IPv6;
	default:
		throw std::invalid_argument("EtherTypeToIPVersion() failed due to unexpected argument");
	}
}

static L4Type LayerToL4Protocol(LayerType type)
{
	if (LayerType2Number(type) != LayerNumber::L4) {
		throw std::invalid_argument("LayerToL4Protocol() failed due to unexpected argument");
	}

	if (!std::holds_alternative<ProtocolType>(type)) {
		return L4Type::Other;
	}

	switch (std::get<ProtocolType>(type)) {
	case ProtocolType::TCP:
		return L4Type::TCP;
	case ProtocolType::UDP:
		return L4Type::UDP;
	case ProtocolType::ICMPv6:
		return L4Type::ICMPv6;
	default:
		return L4Type::Other;
	}
}

PacketInfo PacketBuilder::GetPacketInfo(const RawPacket* rawPacket) const
{
	const std::initializer_list<LayerType> l3Layers = {EtherType::IPv4, EtherType::IPv6};

	PacketInfo info;
	auto layers = Dissect(*rawPacket, LinkType::Ethernet);

	std::optional<size_t> l3Pos = LayersFindFirstOf(layers, l3Layers);
	if (!l3Pos) {
		_logger->error("Unable to locate IPv4/IPv6 layer");
		throw std::runtime_error("PacketBuilder::GetPacketInfo() has failed");
	}

	const Layer& l3Layer = layers[l3Pos.value()];
	info.l3Offset = l3Layer._offset;
	info.l3Type = EtherTypeToIPVersion(std::get<EtherType>(l3Layer._type));

	std::optional<size_t> l3PosNext = LayersFindFirstOf(layers, l3Layers, l3Pos.value() + 1);
	std::optional<size_t> l4Pos = LayersFindFirstOf(layers, LayerNumber::L4, l3Pos.value() + 1);

	if (!l4Pos || (l3PosNext && l3PosNext.value() < l4Pos.value())) {
		// L4 not found or encapsulated within another IPv4/IPv6 packet
		info.l4Offset = 0;
		info.l4Type = L4Type::NotFound;
	} else {
		const Layer& l4Layer = layers[l4Pos.value()];
		info.l4Offset = l4Layer._offset;
		info.l4Type = LayerToL4Protocol(l4Layer._type);
	}

	info.ipAddressesChecksum
		= CalculateIPAddressesChecksum(rawPacket->data, info.l3Type, info.l3Offset);

	return info;
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
