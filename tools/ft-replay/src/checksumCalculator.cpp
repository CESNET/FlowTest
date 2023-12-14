/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the implementation of the CalculateIPAddressesChecksum function.
 */

#include "checksumCalculator.hpp"

#include <netinet/ip.h>
#include <netinet/ip6.h>

namespace replay {

uint16_t CalculateChecksumRaw(const uint8_t* data, size_t length)
{
	// NOLINTBEGIN
	uint32_t sum = 0;

	while (length > 1) {
		sum += 0xFFFF & (*data << 8 | *(data + 1));
		data += 2;
		length -= 2;
	}

	if (length & 1) {
		sum += *((uint8_t*) data);
	}

	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	return static_cast<uint16_t>(sum);
	// NOLINTEND
}

static uint16_t CalculateIpv4PseudoHeader(const Packet& packet)
{
	struct PseudoHeaderV4 {
		uint32_t srcAddr;
		uint32_t dstAddr;
		uint8_t zero;
		uint8_t protocol;
		uint16_t length;
	} __attribute__((packed));

	PseudoHeaderV4 pseudoHeader;

	const iphdr* ipHeader
		= reinterpret_cast<const iphdr*>(packet.data.get() + packet.info.l3Offset);
	pseudoHeader.srcAddr = ipHeader->saddr;
	pseudoHeader.dstAddr = ipHeader->daddr;
	pseudoHeader.zero = 0;
	pseudoHeader.protocol = ipHeader->protocol;
	pseudoHeader.length = htons(packet.dataLen - packet.info.l4Offset);

	return CalculateChecksumRaw(reinterpret_cast<uint8_t*>(&pseudoHeader), sizeof(pseudoHeader));
}

static uint16_t CalculateIpv6PseudoHeader(const Packet& packet)
{
	struct PseudoHeaderV6 {
		struct in6_addr srcAddr;
		struct in6_addr dstAddr;
		uint8_t zero;
		uint8_t protocol;
		uint16_t length;
	} __attribute__((packed));

	PseudoHeaderV6 pseudoHeader;

	const ip6_hdr* ipHeader
		= reinterpret_cast<const ip6_hdr*>(packet.data.get() + packet.info.l3Offset);
	pseudoHeader.srcAddr = ipHeader->ip6_src;
	pseudoHeader.dstAddr = ipHeader->ip6_dst;
	pseudoHeader.zero = 0;
	pseudoHeader.protocol = ipHeader->ip6_nxt;
	pseudoHeader.length = htons(packet.dataLen - packet.info.l4Offset);

	return CalculateChecksumRaw(reinterpret_cast<uint8_t*>(&pseudoHeader), sizeof(pseudoHeader));
}

uint16_t CalculateIPAddressesChecksum(const std::byte* ptr, enum L3Type l3Type, uint16_t l3Offset)
{
	size_t length = 0;
	const uint8_t* data = nullptr;

	if (l3Type == L3Type::IPv4) {
		const iphdr* ipHeader = reinterpret_cast<const iphdr*>(ptr + l3Offset);
		length = sizeof(ipHeader->saddr) + sizeof(ipHeader->daddr);
		data = reinterpret_cast<const uint8_t*>(&ipHeader->saddr);
	} else if (l3Type == L3Type::IPv6) {
		const ip6_hdr* ip6Header = reinterpret_cast<const ip6_hdr*>(ptr + l3Offset);
		length = sizeof(ip6Header->ip6_src) + sizeof(ip6Header->ip6_dst);
		data = reinterpret_cast<const uint8_t*>(&ip6Header->ip6_src);
	}

	return CalculateChecksumRaw(reinterpret_cast<const uint8_t*>(data), length) ^ 0xFFFF;
}

uint16_t CalculatePsedoHeaderChecksum(const Packet& packet)
{
	if (packet.info.l3Type == L3Type::IPv4) {
		return CalculateIpv4PseudoHeader(packet);
	}
	return CalculateIpv6PseudoHeader(packet);
}

} // namespace replay
