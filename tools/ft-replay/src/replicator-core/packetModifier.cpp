/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief packet modifier
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetModifier.hpp"

#include "ipAddress.hpp"
#include "macAddress.hpp"

#include <algorithm>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

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
}

} // namespace replay
