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
#include "timeval.h"

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
using IPAddressVariant = std::variant<std::monostate, IPv4Address, IPv6Address>;

/**
 * @brief The flow record
 *
 */
struct FlowRecord {
	L3Protocol _l3Proto;
	L4Protocol _l4Proto;

	Timeval _fwdFirstTs;
	Timeval _fwdLastTs;
	MacAddress _fwdMacAddr;
	IPAddressVariant _fwdIpAddr;
	uint64_t _fwdPkts = 0;
	uint64_t _fwdBytes = 0;
	uint16_t _fwdPort = 0;
	uint64_t _desiredFwdPkts = 0;
	uint64_t _desiredFwdBytes = 0;

	Timeval _revFirstTs;
	Timeval _revLastTs;
	MacAddress _revMacAddr;
	IPAddressVariant _revIpAddr;
	uint64_t _revPkts = 0;
	uint64_t _revBytes = 0;
	uint16_t _revPort = 0;
	uint64_t _desiredRevPkts = 0;
	uint64_t _desiredRevBytes = 0;
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
	void RecordPacket(uint64_t flowId, Timeval time, Direction dir, const PcppPacket& packet);

	/**
	 * @brief Write out the information about the recorded flows and packets to a csv file
	 *
	 * @param fileName Path to the resulting output file
	 *
	 * @throws std::runtime_error on failure when writing the output file
	 */
	void WriteReportCsv(const std::string& fileName);

	/**
	 * @brief Print out statistics comparing the recorded data with the profiles
	 */
	void PrintComparisonStats() const;

private:
	std::vector<FlowRecord> _records; //< The flow records

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("TrafficMeter");

	void ExtractPacketParams(
		const PcppPacket& packet,
		L3Protocol l3Proto,
		L4Protocol l4Proto,
		MacAddress& srcMac,
		IPAddressVariant& srcIp,
		uint16_t& srcPort,
		MacAddress& dstMac,
		IPAddressVariant& dstIp,
		uint16_t& dstPort);
};

} // namespace generator
