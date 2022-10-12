/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Ipv4 layer planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"

#include <pcapplusplus/IpAddress.h>

namespace generator {

/**
 * @brief A representation of a IPv4 layer.
 */
class IPv4 : public Layer {
public:

	using IPv4Address = pcpp::IPv4Address;

	/**
	 * @brief Construct a new IPv4 object
	 *
	 * @param ipSrc Source IP address
	 * @param ipDst Destination IP address
	 */
	IPv4(IPv4Address ipSrc, IPv4Address ipDst);

	void PlanFlow(Flow& flow) override;

	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	IPv4Address _ipSrc;
	IPv4Address _ipDst;
	uint8_t _ttl;
	uint16_t _fwdId = 0;
	uint16_t _revId = 0;

};

} // namespace generator
