/*
 * @file
 * @brief Packet dissector
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dissector.hpp"

#include <converters.h>
#include <protocol/ethernet.hpp>
#include <protocol/icmp6.hpp>
#include <protocol/ipFragmentType.hpp>
#include <protocol/ipv4.hpp>
#include <protocol/ipv6.hpp>
#include <protocol/ipv6DestOptions.hpp>
#include <protocol/ipv6Fragment.hpp>
#include <protocol/ipv6HopByHop.hpp>
#include <protocol/ipv6Routing.hpp>
#include <protocol/mpls.hpp>
#include <protocol/tcp.hpp>
#include <protocol/udp.hpp>
#include <protocol/vlan.hpp>

#include <cstddef>

using namespace replay::protocol;

namespace replay::dissector {

struct DissectorCtx {
	/** Packet */
	const RawPacket& _packet;
	/** Already parsed layers */
	std::vector<Layer> _layers;
};

static void ProcessLinkTypeLayer(DissectorCtx& ctx, size_t offset, LinkType layer);
static void ProcessEtherTypeLayer(DissectorCtx& ctx, size_t offset, EtherType layer);
static void ProcessProtocolTypeLayer(DissectorCtx& ctx, size_t offset, ProtocolType layer);
static void ProcessPayloadTypeLayer(DissectorCtx& ctx, size_t offset, PayloadType layer);
static void ProcessIPv4(DissectorCtx& ctx, size_t offset);
static void ProcessIPv6(DissectorCtx& ctx, size_t offset);

template <typename... Args>
static void LayerPush(DissectorCtx& ctx, Args... args)
{
	ctx._layers.emplace_back(Layer {args...});
}

template <typename T>
static const T* PacketCast(const DissectorCtx& ctx, size_t offset) noexcept
{
	return reinterpret_cast<const T*>(ctx._packet.data + offset);
}

/**
 * @brief Assert that the dissected packet has at least @p minSize bytes available.
 * @param[in] ctx     Dissector context
 * @param[in] offset  Packet offset from where to start
 * @param[in] minSize Minimum required number of bytes from @p offset
 * @throw DissectorException if the assertion is not satisfied
 */
static void AssertBytesAvailable(const DissectorCtx& ctx, size_t offset, size_t minSize)
{
	const size_t captureLength = ctx._packet.dataLen;
	const size_t remain = (captureLength > offset) ? (captureLength - offset) : 0;

	if (remain >= minSize) {
		return;
	}

	throw DissectorException("unexpected end of packet");
}

static void
ProcessIPFragment(DissectorCtx& ctx, size_t offset, IPFragmentType type, ProtocolType nextProtoId)
{
	switch (type) {
	case IPFragmentType::None:
	case IPFragmentType::First:
		return ProcessProtocolTypeLayer(ctx, offset, nextProtoId);
	case IPFragmentType::Middle:
	case IPFragmentType::Last:
		return ProcessPayloadTypeLayer(ctx, offset, PayloadType::IPFragment);
	}
}

static void ProcessEthernet(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, Ethernet::HEADER_SIZE);

	const auto* header = PacketCast<Ethernet>(ctx, offset);
	const size_t nextOffset = offset + Ethernet::HEADER_SIZE;
	const uint16_t nextEthertype = be16toh(header->_ethertype);

	if (!header->IsValid()) {
		throw DissectorException("invalid Ethernet header");
	}

	LayerPush(ctx, LinkType::Ethernet, offset);
	return ProcessEtherTypeLayer(ctx, nextOffset, EtherType(nextEthertype));
}

static void ProcessVLAN(DissectorCtx& ctx, size_t offset, EtherType type)
{
	AssertBytesAvailable(ctx, offset, VLAN::HEADER_SIZE);

	const auto* header = PacketCast<VLAN>(ctx, offset);
	const size_t nextOffset = offset + VLAN::HEADER_SIZE;
	const uint16_t nextEthertype = be16toh(header->_Ethertype);

	LayerPush(ctx, type, offset);
	return ProcessEtherTypeLayer(ctx, nextOffset, EtherType(nextEthertype));
}

static void ProcessMPLS(DissectorCtx& ctx, size_t offset, EtherType type)
{
	AssertBytesAvailable(ctx, offset, MPLS::HEADER_SIZE);

	const auto* header = PacketCast<MPLS>(ctx, offset);
	size_t nextOffset = offset + MPLS::HEADER_SIZE;

	LayerPush(ctx, type, offset);

	if (!header->IsBosSet()) {
		// Additional MPLS labels are present
		return ProcessMPLS(ctx, nextOffset, type);
	}

	/*
	 * MPLS doesn't have information about the next header, but usually it is an IP.
	 * Look ahead to the following nibble (i.e. first 4 bits) to determine the next protocol.
	 */
	AssertBytesAvailable(ctx, nextOffset, 1U);

	const uint8_t* byte = PacketCast<uint8_t>(ctx, nextOffset);
	const uint8_t ipVersion = ((*byte) >> 4) & 0x0F;

	switch (ipVersion) {
	case IPv4::VERSION:
		return ProcessIPv4(ctx, nextOffset);
	case IPv6::VERSION:
		return ProcessIPv6(ctx, nextOffset);
	default:
		throw DissectorException("unknown protocol after the last MPLS label");
	}
}

static void ProcessIPv4(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv4::HEADER_SIZE_MIN);

	const auto* header = PacketCast<IPv4>(ctx, offset);
	const uint16_t realHeaderLength = header->GetHdrLength();
	const size_t nextOffset = offset + realHeaderLength;
	const ProtocolType nextProto {header->_nextProtoId};

	if (!header->IsValid()) {
		throw DissectorException("invalid IPv4 header");
	}

	AssertBytesAvailable(ctx, offset, realHeaderLength);
	LayerPush(ctx, EtherType::IPv4, offset);
	return ProcessIPFragment(ctx, nextOffset, header->GetFragmentType(), nextProto);
}

static void ProcessIPv6(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv6::HEADER_SIZE);

	const auto* header = PacketCast<IPv6>(ctx, offset);
	const size_t nextOffset = offset + IPv6::HEADER_SIZE;
	const ProtocolType nextProto {header->_nextProtoId};

	if (!header->IsValid()) {
		throw DissectorException("invalid IPv6 header");
	}

	LayerPush(ctx, EtherType::IPv6, offset);
	return ProcessProtocolTypeLayer(ctx, nextOffset, nextProto);
}

static void ProcessTCP(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, TCP::HEADER_SIZE_MIN);

	const auto* header = PacketCast<TCP>(ctx, offset);
	const uint16_t realHeaderLength = header->GetHdrLength();
	const size_t nextOffset = offset + realHeaderLength;

	if (!header->IsValid()) {
		throw DissectorException("invalid TCP header");
	}

	AssertBytesAvailable(ctx, offset, realHeaderLength);
	LayerPush(ctx, ProtocolType::TCP, offset);
	return ProcessPayloadTypeLayer(ctx, nextOffset, PayloadType::AppData);
}

static void ProcessUDP(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, UDP::HEADER_SIZE);

	const auto* header = PacketCast<UDP>(ctx, offset);
	const size_t nextOffset = offset + UDP::HEADER_SIZE;

	if (!header->IsValid()) {
		throw DissectorException("invalid UDP header");
	}

	LayerPush(ctx, ProtocolType::UDP, offset);
	return ProcessPayloadTypeLayer(ctx, nextOffset, PayloadType::AppData);
}

static void ProcessICMPv6(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, ICMPv6::HEADER_SIZE);

	LayerPush(ctx, ProtocolType::ICMPv6, offset);
}

static void ProcessIPv6HopByHopOption(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv6HopByHop::HEADER_SIZE_MIN);

	const auto* header = PacketCast<IPv6HopByHop>(ctx, offset);
	const uint16_t realHeaderLength = header->GetHdrLength();
	const size_t nextOffset = offset + realHeaderLength;
	const ProtocolType nextProto {header->_nextProtoId};

	AssertBytesAvailable(ctx, offset, realHeaderLength);
	LayerPush(ctx, ProtocolType::IPv6HopOpt, offset);
	return ProcessProtocolTypeLayer(ctx, nextOffset, nextProto);
}

static void ProcessIPv6RouteOption(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv6Routing::HEADER_SIZE_MIN);

	const auto* header = PacketCast<IPv6Routing>(ctx, offset);
	const uint16_t realHeaderLength = header->GetHdrLength();
	const size_t nextOffset = offset + realHeaderLength;
	const ProtocolType nextProto {header->_nextProtoId};

	AssertBytesAvailable(ctx, offset, realHeaderLength);
	LayerPush(ctx, ProtocolType::IPv6Route, offset);
	return ProcessProtocolTypeLayer(ctx, nextOffset, nextProto);
}

static void ProcessIPv6FragmentOption(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv6Fragment::HEADER_SIZE);

	const auto* header = PacketCast<IPv6Fragment>(ctx, offset);
	const size_t nextOffset = offset + IPv6Fragment::HEADER_SIZE;
	const ProtocolType nextProto {header->_nextProtoId};

	LayerPush(ctx, ProtocolType::IPv6Frag, offset);
	return ProcessIPFragment(ctx, nextOffset, header->GetFragmentType(), nextProto);
}

static void ProcessIPv6DestOption(DissectorCtx& ctx, size_t offset)
{
	AssertBytesAvailable(ctx, offset, IPv6DestOptions::HEADER_SIZE_MIN);

	const auto* header = PacketCast<IPv6DestOptions>(ctx, offset);
	const uint16_t realHeaderLength = header->GetHdrLength();
	const size_t nextOffset = offset + realHeaderLength;
	const ProtocolType nextProto {header->_nextProtoId};

	AssertBytesAvailable(ctx, offset, realHeaderLength);
	LayerPush(ctx, ProtocolType::IPv6Dest, offset);
	return ProcessProtocolTypeLayer(ctx, nextOffset, nextProto);
}

static void ProcessLinkTypeLayer(DissectorCtx& ctx, size_t offset, LinkType type)
{
	switch (type) {
	case LinkType::Ethernet:
		return ProcessEthernet(ctx, offset);
	default:
		throw DissectorException("unsupported link layer");
	}
}

static void ProcessEtherTypeLayer(DissectorCtx& ctx, size_t offset, EtherType type)
{
	switch (type) {
	case EtherType::IPv4:
		return ProcessIPv4(ctx, offset);
	case EtherType::IPv6:
		return ProcessIPv6(ctx, offset);
	case EtherType::VLAN:
	case EtherType::VLANSTag:
		return ProcessVLAN(ctx, offset, type);
	case EtherType::MPLS:
	case EtherType::MPLSUpstream:
		return ProcessMPLS(ctx, offset, type);
	default: {
		const std::string typeStr = ft::ConvertUintToHex(static_cast<uint16_t>(type));
		throw DissectorException("unsupported EtherType " + typeStr);
	}
	}
}

static void ProcessProtocolTypeLayer(DissectorCtx& ctx, size_t offset, ProtocolType type)
{
	switch (type) {
	case ProtocolType::IPv6HopOpt:
		return ProcessIPv6HopByHopOption(ctx, offset);
	case ProtocolType::IPv4: // Tunneled IPv4
		return ProcessIPv4(ctx, offset);
	case ProtocolType::TCP:
		return ProcessTCP(ctx, offset);
	case ProtocolType::UDP:
		return ProcessUDP(ctx, offset);
	case ProtocolType::ICMPv6:
		return ProcessICMPv6(ctx, offset);
	case ProtocolType::IPv6: // Tunneled IPv6
		return ProcessIPv6(ctx, offset);
	case ProtocolType::IPv6Route:
		return ProcessIPv6RouteOption(ctx, offset);
	case ProtocolType::IPv6Frag:
		return ProcessIPv6FragmentOption(ctx, offset);
	case ProtocolType::IPv6Dest:
		return ProcessIPv6DestOption(ctx, offset);
	case ProtocolType::IPv6NoNext:
		return; // Nothing to do
	default:
		return LayerPush(ctx, ProtocolType::Unknown, offset);
	}
}

static void ProcessPayloadTypeLayer(DissectorCtx& ctx, size_t offset, PayloadType layer)
{
	switch (layer) {
	case PayloadType::AppData:
	case PayloadType::IPFragment:
	case PayloadType::Unknown:
		return LayerPush(ctx, layer, offset);
	}
}

std::vector<Layer> Dissect(const RawPacket& packet, LayerType firstLayer)
{
	DissectorCtx ctx {packet, {}};

	if (std::holds_alternative<LinkType>(firstLayer)) {
		ProcessLinkTypeLayer(ctx, 0, std::get<LinkType>(firstLayer));
	} else if (std::holds_alternative<EtherType>(firstLayer)) {
		ProcessEtherTypeLayer(ctx, 0, std::get<EtherType>(firstLayer));
	} else if (std::holds_alternative<ProtocolType>(firstLayer)) {
		ProcessProtocolTypeLayer(ctx, 0, std::get<ProtocolType>(firstLayer));
	} else if (std::holds_alternative<PayloadType>(firstLayer)) {
		ProcessPayloadTypeLayer(ctx, 0, std::get<PayloadType>(firstLayer));
	} else {
		throw DissectorException("unknown layer type to process");
	}

	return ctx._layers;
}

} // namespace replay::dissector
