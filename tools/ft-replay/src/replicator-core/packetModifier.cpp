/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief packet modifier
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetModifier.hpp"

#include "checksumCalculator.hpp"
#include "ipAddress.hpp"
#include "macAddress.hpp"

#include <algorithm>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

namespace replay {

PacketModifier::PacketModifier(ModifierStrategies modifierStrategies)
	: _strategy(std::move(modifierStrategies))
{
	std::sort(_strategy.loopOnly.begin(), _strategy.loopOnly.end());
}

bool PacketModifier::IsEnabledThisLoop(size_t loopId) noexcept
{
	if (_strategy.loopOnly.empty()) {
		return true;
	}

	return std::binary_search(_strategy.loopOnly.begin(), _strategy.loopOnly.end(), loopId);
}

void PacketModifier::SetChecksumOffloads(const ChecksumOffloads& checksumOffloads) noexcept
{
	_checksumOffloads = checksumOffloads;
}

void PacketModifier::Modify(std::byte* ptr, const PacketInfo& pktInfo, size_t loopId)
{
	ether_header* ethHeader = reinterpret_cast<ether_header*>(ptr);
	_strategy.unitSrcMac->ApplyStrategy((MacAddress::MacPtr) ethHeader->ether_shost);
	_strategy.unitDstMac->ApplyStrategy((MacAddress::MacPtr) ethHeader->ether_dhost);

	if (pktInfo.l3Type == L3Type::IPv4) {
		iphdr* ipHeader = reinterpret_cast<iphdr*>(ptr + pktInfo.l3Offset);
		_strategy.unitSrcIp->ApplyStrategy((IpAddressView::Ip4Ptr) &ipHeader->saddr);
		_strategy.loopSrcIp->ApplyStrategy((IpAddressView::Ip4Ptr) &ipHeader->saddr, loopId);
		_strategy.unitDstIp->ApplyStrategy((IpAddressView::Ip4Ptr) &ipHeader->daddr);
		_strategy.loopDstIp->ApplyStrategy((IpAddressView::Ip4Ptr) &ipHeader->daddr, loopId);
	} else {
		ip6_hdr* ipHeader = reinterpret_cast<ip6_hdr*>(ptr + pktInfo.l3Offset);
		_strategy.unitSrcIp->ApplyStrategy((IpAddressView::Ip6Ptr) &ipHeader->ip6_src);
		_strategy.loopSrcIp->ApplyStrategy((IpAddressView::Ip6Ptr) &ipHeader->ip6_src, loopId);
		_strategy.unitDstIp->ApplyStrategy((IpAddressView::Ip6Ptr) &ipHeader->ip6_dst);
		_strategy.loopDstIp->ApplyStrategy((IpAddressView::Ip6Ptr) &ipHeader->ip6_dst, loopId);
	}

	UpdateChecksumOffloads(ptr, pktInfo);
}

void PacketModifier::UpdateChecksumOffloads(std::byte* ptr, const PacketInfo& pktInfo)
{
	if (!_checksumOffloads.checksumIPv4 && !_checksumOffloads.checksumTCP
		&& !_checksumOffloads.checksumUDP) {
		return;
	}

	uint16_t newIPAddresesChecksum
		= CalculateIPAddressesChecksum(ptr, pktInfo.l3Type, pktInfo.l3Offset);

	if (_checksumOffloads.checksumIPv4 && pktInfo.l3Type == L3Type::IPv4) {
		iphdr* ipHeader = reinterpret_cast<iphdr*>(ptr + pktInfo.l3Offset);
		uint16_t newChecksum = CalculateChecksum(
			ntohs(ipHeader->check),
			pktInfo.ipAddressesChecksum,
			newIPAddresesChecksum);
		ipHeader->check = htons(newChecksum);
	}
	if (_checksumOffloads.checksumTCP && pktInfo.l4Type == L4Type::TCP) {
		tcphdr* tcpHeader = reinterpret_cast<tcphdr*>(ptr + pktInfo.l4Offset);
		uint16_t newChecksum = CalculateChecksum(
			ntohs(tcpHeader->check),
			pktInfo.ipAddressesChecksum,
			newIPAddresesChecksum);
		tcpHeader->check = htons(newChecksum);
	} else if (_checksumOffloads.checksumUDP && pktInfo.l4Type == L4Type::UDP) {
		udphdr* udpHeader = reinterpret_cast<udphdr*>(ptr + pktInfo.l4Offset);
		uint16_t newChecksum = CalculateChecksum(
			ntohs(udpHeader->check),
			pktInfo.ipAddressesChecksum,
			newIPAddresesChecksum,
			true);
		udpHeader->check = htons(newChecksum);
	} else if (_checksumOffloads.checksumICMPv6 && pktInfo.l4Type == L4Type::ICMPv6) {
		icmp6_hdr* icmp6Header = reinterpret_cast<icmp6_hdr*>(ptr + pktInfo.l4Offset);
		uint16_t newChecksum = CalculateChecksum(
			ntohs(icmp6Header->icmp6_cksum),
			pktInfo.ipAddressesChecksum,
			newIPAddresesChecksum);
		icmp6Header->icmp6_cksum = htons(newChecksum);
	}
}

uint16_t PacketModifier::CalculateChecksum(
	uint16_t originalChecksum,
	uint16_t originalIpChecksum,
	uint16_t newIpChecksum,
	bool isUDP) const noexcept
{
	int32_t checksum = originalChecksum - originalIpChecksum + newIpChecksum;
	if (checksum < 0) {
		checksum -= 1;
	} else if (checksum >= std::numeric_limits<uint16_t>::max()) {
		checksum += 1;
	}

	/**
	 * UDP has a special case where 0x0000 is reserved for "no checksum computed".
	 * Thus for UDP, 0x0000 is illegal and when calculated following the standard
	 * algorithm, replaced with 0xffff.
	 */
	if (!static_cast<uint16_t>(checksum) && isUDP) {
		checksum = 0xFFFF;
	}

	return static_cast<uint16_t>(checksum);
}

} // namespace replay
