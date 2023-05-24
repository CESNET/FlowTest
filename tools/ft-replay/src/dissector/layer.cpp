/*
 * @file
 * @brief Packet layer
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dissector.hpp"

#include <stdexcept>

namespace replay::dissector {

static LayerNumber LinkType2Number(LinkType type)
{
	switch (type) {
	case LinkType::Ethernet:
		return LayerNumber::L2;
	}

	throw std::runtime_error("LinkType2Number() has failed: unknown type");
}

static LayerNumber EtherType2Number(EtherType type)
{
	switch (type) {
	case EtherType::VLAN:
	case EtherType::MPLS:
	case EtherType::MPLSUpstream:
	case EtherType::VLANSTag:
		return LayerNumber::L2;
	case EtherType::IPv4:
	case EtherType::IPv6:
		return LayerNumber::L3;
	}

	throw std::runtime_error("EtherType2Number() has failed: unknown type");
}

static LayerNumber ProtocolType2Number(ProtocolType type)
{
	switch (type) {
	case ProtocolType::IPv6HopOpt:
	case ProtocolType::IPv4:
	case ProtocolType::IPv6:
	case ProtocolType::IPv6Route:
	case ProtocolType::IPv6Frag:
	case ProtocolType::IPv6NoNext:
	case ProtocolType::IPv6Dest:
		return LayerNumber::L3;
	case ProtocolType::TCP:
	case ProtocolType::UDP:
		return LayerNumber::L4;
	case ProtocolType::Unknown:
		// Consider the "worst" case as there is no other detected layer after this one.
		return LayerNumber::L7;
	}

	throw std::runtime_error("ProtocolType2Number() has failed: unknown type");
}

static LayerNumber PayloadType2Number(PayloadType type)
{
	switch (type) {
	case PayloadType::Unknown:
	case PayloadType::IPFragment:
	case PayloadType::AppData:
		return LayerNumber::L7;
	}

	throw std::runtime_error("PayloadType2Number() has failed: unknown type");
}

LayerNumber LayerType2Number(LayerType type)
{
	if (std::holds_alternative<LinkType>(type)) {
		return LinkType2Number(std::get<LinkType>(type));
	} else if (std::holds_alternative<EtherType>(type)) {
		return EtherType2Number(std::get<EtherType>(type));
	} else if (std::holds_alternative<ProtocolType>(type)) {
		return ProtocolType2Number(std::get<ProtocolType>(type));
	} else if (std::holds_alternative<PayloadType>(type)) {
		return PayloadType2Number(std::get<PayloadType>(type));
	} else {
		throw std::runtime_error("LayerType2Number() has failed: unsupported layer type");
	}
}

} // namespace replay::dissector
