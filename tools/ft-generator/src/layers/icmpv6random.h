/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief ICMPv6 layer planner using random messages
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../flowplanhelper.h"
#include "../layer.h"
#include "../packet.h"
#include "../packetflowspan.h"
#include "../pcpppacket.h"

#include <cstdint>
#include <queue>
#include <vector>

namespace generator {

/**
 * @brief ICMPv6 destination unreachable codes
 */
enum class Icmpv6DestUnreachableCodes : uint8_t {
	NoRoute = 0, // No route to destination
	Prohibited = 1, // Communication with destination administratively prohibited
	BeyondScope = 2, // Beyond scope of source address
	AddrUnreachable = 3, // Address unreachable
	PortUnreachable = 4, // Port unreachable
	SourceAddrPolicyFailed = 5, // Source address failed ingress/egress policy
	RejectRoute = 6, // Reject route to destination
	ErrorInSrcRoutingHdr = 7, // Error in Source Routing Header
	HdrTooLong = 8 // Headers too long
};

/**
 * @brief A representation of an Icmpv6Random layer.
 */
class Icmpv6Random : public Layer {
public:
	/**
	 * @brief Construct a new Icmpv6Random object
	 *
	 */
	Icmpv6Random();

	/**
	 * @brief Plan Icmpv6Random layer
	 *
	 * @param flow Flow to plan.
	 */
	virtual void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build Icmpv6Random layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	virtual void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	Icmpv6DestUnreachableCodes _destUnreachableCode;
};

} // namespace generator
