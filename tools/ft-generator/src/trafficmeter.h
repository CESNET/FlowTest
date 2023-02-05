/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Traffic meter
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "flowprofile.h"
#include "logger.h"
#include "packet.h"
#include "pcpppacket.h"

#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/IpAddress.h>
#include <pcapplusplus/Layer.h>
#include <pcapplusplus/ProtocolType.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>

#include <ctime>
#include <variant>
#include <vector>

namespace generator {

using MacAddress = pcpp::MacAddress;
using IPv4Address = pcpp::IPv4Address;
using IPv6Address = pcpp::IPv6Address;
using IPAddress = std::variant<std::monostate, IPv4Address, IPv6Address>;

/**
 * @brief The flow record
 *
 */
struct FlowRecord {
	L3Protocol _l3Proto;
	L4Protocol _l4Proto;

	timeval _firstTs = {0, 0};
	timeval _lastTs = {0, 0};

	MacAddress _fwdMacAddr;
	IPAddress _fwdIpAddr;
	uint64_t _fwdPkts = 0;
	uint64_t _fwdBytes = 0;
	uint16_t _fwdPort = 0;

	MacAddress _revMacAddr;
	IPAddress _revIpAddr;
	uint64_t _revPkts = 0;
	uint64_t _revBytes = 0;
	uint16_t _revPort = 0;
};

/**
 * @brief Traffic metering component
 *
 * Records packets generated for the flows and keeps statistics about them to provide
 * a summary of the generated traffic
 */
class TrafficMeter {
public:
	/**
	 * @brief Open a new flow for metering
	 *
	 * For implementation effectivity reasons, flow ID is currently required to
	 * be a number starting at 0 with the first call of this method, and
	 * increasing by 1 by each subsequential call!
	 *
	 * @param flowId  The ID of the flow
	 * @param profile The flow profile
	 *
	 * @throws std::runtime_error  When an invalid flow ID is provided
	 */
	void OpenFlow(uint64_t flowId, const FlowProfile& profile);

	/**
	 * @brief Close a flow
	 *
	 * @param flowId  The ID of the flow
	 */
	void CloseFlow(uint64_t flowId);

	/**
	 * @brief Update the flow record
	 *
	 * @param flowId  The ID of the flow
	 * @param time    The timestamp of the packet
	 * @param dir     The direction of the packet
	 * @param packet  The generated packet
	 */
	void RecordPacket(uint64_t flowId, timeval time, Direction dir, const PcppPacket& packet);

	/**
	 * @brief Write out summary of the recorded flows and packets
	 *
	 */
	void WriteReport();

	/**
	 * @brief Write out summary of the recorded flows and packets to a csv file
	 *
	 * @param fileName Path to the resulting output file
	 *
	 * @throws std::runtime_error on failure when writing the output file
	 */
	void WriteReportCsv(const std::string& fileName);

private:
	std::vector<FlowRecord> _records; //< The flow records

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("TrafficMeter");

	void ExtractPacketParams(
		const PcppPacket& packet,
		L3Protocol l3Proto,
		L4Protocol l4Proto,
		MacAddress& srcMac,
		IPAddress& srcIp,
		uint16_t& srcPort,
		MacAddress& dstMac,
		IPAddress& dstIp,
		uint16_t& dstPort);
};

} // namespace generator
