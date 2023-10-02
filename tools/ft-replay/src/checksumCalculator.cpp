/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the implementation of the CalculateIPAddressesChecksum function.
 */

#include "checksumCalculator.hpp"

#include <netinet/ip.h>
#include <netinet/ip6.h>

namespace replay {

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

	uint32_t sum = 0;

	while (length > 1) {
		sum += 0xFFFF & (*data << 8 | *(data + 1));
		data += 2;
		length -= 2;
	}

	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return ((uint16_t) sum ^ 0xFFFF);
}

} // namespace replay
