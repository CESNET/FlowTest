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

#include <pcapplusplus/IPv4Layer.h>
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
	 * @param fragmentChance Fragmentation chance
	 * @param minPacketSizeToFragment Minimal size of a packet to be chosen for fragmentation
	 */
	IPv4(
		IPv4Address ipSrc,
		IPv4Address ipDst,
		double fragmentChance,
		uint16_t minPacketSizeToFragment);

	void PlanFlow(Flow& flow) override;

	void PostPlanFlow(Flow& flow) override;

	void PlanExtra(Flow& flow) override;

	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

	void PostBuild(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

	size_t SizeUpToIpLayer(Packet& packet) const override;

private:
	IPv4Address _ipSrc;
	IPv4Address _ipDst;

	uint8_t _ttl;

	uint16_t _fwdId = 0;
	uint16_t _revId = 0;

	int _buildPacketSize = 0;
	double _fragmentChance = 0.0;
	double _minPacketSizeToFragment = 0;
	int _fragmentCount = 0;
	int _fragmentRemaining = 0;
	std::vector<uint8_t> _fragmentBuffer;
	int _fragmentOffset = 0;
	uint16_t _fragmentId = 0;
	uint8_t _fragmentProto = 0;

	void BuildFragment(PcppPacket& packet, pcpp::iphdr* ipHdr);
};

} // namespace generator
