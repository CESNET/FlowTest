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
 * @brief A representation of a IPv6 layer.
 */
class IPv6 : public Layer {
public:

	/**
	 * @brief Construct a new IPv6 object
	 *
	 * @param ipSrc Source IP address
	 * @param ipDst Destination IP address
	 */
	IPv6(IPv6Address ipSrc, IPv6Address ipDst);

	void PlanFlow(Flow& flow) override;

	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	IPv6Address _ipSrc;
	IPv6Address _ipDst;
	uint8_t _ttl;
	uint8_t _fwdFlowLabel[3];
	uint8_t _revFlowLabel[3];
};

} // namespace generator
