/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Traffic meter
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "trafficmeter.h"

#include <cstring>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace generator {

void TrafficMeter::OpenFlow(uint64_t flowId, const FlowProfile& profile)
{
	if (flowId != _records.size()) {
		// Assume the flowIds go in order, so we can just use a vector
		throw std::runtime_error("Unexpected flow ID");
	}

	FlowRecord rec;
	rec._l3Proto = profile._l3Proto;
	rec._l4Proto = profile._l4Proto;
	_records.push_back(rec);
}

void TrafficMeter::CloseFlow(uint64_t flowId)
{
	(void) flowId;
	// No need to do anything for now...
}

void TrafficMeter::ExtractPacketParams(
	const PcppPacket& packet,
	L3Protocol l3Proto,
	L4Protocol l4Proto,
	MacAddress& srcMac,
	IPAddressVariant& srcIp,
	uint16_t& srcPort,
	MacAddress& dstMac,
	IPAddressVariant& dstIp,
	uint16_t& dstPort)
{
	pcpp::EthLayer* ethLayer = static_cast<pcpp::EthLayer*>(packet.getLayerOfType(pcpp::Ethernet));
	assert(ethLayer != nullptr);
	srcMac = ethLayer->getSourceMac();
	dstMac = ethLayer->getDestMac();

	if (l3Proto == L3Protocol::Ipv4) {
		pcpp::IPv4Layer* ipv4Layer
			= static_cast<pcpp::IPv4Layer*>(packet.getLayerOfType(pcpp::IPv4));
		assert(ipv4Layer != nullptr);
		srcIp = ipv4Layer->getSrcIPv4Address();
		dstIp = ipv4Layer->getDstIPv4Address();
	} else if (l3Proto == L3Protocol::Ipv6) {
		pcpp::IPv6Layer* ipv6Layer
			= static_cast<pcpp::IPv6Layer*>(packet.getLayerOfType(pcpp::IPv6));
		assert(ipv6Layer != nullptr);
		srcIp = ipv6Layer->getSrcIPv6Address();
		dstIp = ipv6Layer->getDstIPv6Address();
	}

	if (l4Proto == L4Protocol::Tcp) {
		pcpp::TcpLayer* tcpLayer = static_cast<pcpp::TcpLayer*>(packet.getLayerOfType(pcpp::TCP));
		assert(tcpLayer != nullptr);
		srcPort = tcpLayer->getSrcPort();
		dstPort = tcpLayer->getDstPort();
	} else if (l4Proto == L4Protocol::Udp) {
		pcpp::UdpLayer* udpLayer = static_cast<pcpp::UdpLayer*>(packet.getLayerOfType(pcpp::UDP));
		assert(udpLayer != nullptr);
		srcPort = udpLayer->getSrcPort();
		dstPort = udpLayer->getDstPort();
	}
}

static uint64_t GetPacketSizeFromIPLayer(const PcppPacket& packet)
{
	const pcpp::Layer* layer = packet.getFirstLayer();

	while (layer != nullptr) {
		if (layer->getProtocol() == pcpp::IPv4 || layer->getProtocol() == pcpp::IPv6) {
			return layer
				->getDataLen(); // Length from the first byte of the header to the end of the packet
		}
		layer = layer->getNextLayer();
	}

	return 0;
}

void TrafficMeter::RecordPacket(
	uint64_t flowId,
	timeval time,
	Direction dir,
	const PcppPacket& packet)
{
	assert(flowId < _records.size());
	FlowRecord& rec = _records[flowId];

	if (rec._fwdPkts == 0 && rec._revPkts == 0) {
		rec._firstTs = time;
	}
	rec._lastTs = time;

	assert(dir != Direction::Unknown);
	if (dir == Direction::Forward) {
		if (rec._fwdPkts == 0 && rec._revPkts == 0) {
			ExtractPacketParams(
				packet,
				rec._l3Proto,
				rec._l4Proto,
				rec._fwdMacAddr,
				rec._fwdIpAddr,
				rec._fwdPort,
				rec._revMacAddr,
				rec._revIpAddr,
				rec._revPort);
		}
		rec._fwdPkts++;
		rec._fwdBytes += GetPacketSizeFromIPLayer(packet);

	} else if (dir == Direction::Reverse) {
		if (rec._fwdPkts == 0 && rec._revPkts == 0) {
			ExtractPacketParams(
				packet,
				rec._l3Proto,
				rec._l4Proto,
				rec._revMacAddr,
				rec._revIpAddr,
				rec._revPort,
				rec._fwdMacAddr,
				rec._fwdIpAddr,
				rec._fwdPort);
		}
		rec._revPkts++;
		rec._revBytes += GetPacketSizeFromIPLayer(packet);
	}
}

static int64_t TimevalToMicroseconds(const timeval& tv)
{
	int64_t microseconds = 0;

	if (tv.tv_sec > std::numeric_limits<int64_t>::max() / 1000000
		|| tv.tv_sec < std::numeric_limits<int64_t>::min() / 1000000) {
		throw std::runtime_error("cannot convert timeval to microseconds due to overflow");
	}
	microseconds += tv.tv_sec * 1000000;

	if ((microseconds > 0 && std::numeric_limits<int64_t>::max() - microseconds < tv.tv_usec)
		|| (microseconds < 0 && tv.tv_usec < std::numeric_limits<int64_t>::min() - microseconds)) {
		throw std::runtime_error("cannot convert timeval to microseconds due to overflow");
	}
	microseconds += tv.tv_usec;

	return microseconds;
}

void TrafficMeter::WriteReportCsv(const std::string& fileName)
{
	errno = 0;
	std::ofstream csvFile(fileName);
	if (!csvFile.is_open()) {
		const std::string& err = errno != 0 ? std::strerror(errno) : "Unknown error";
		throw std::runtime_error("Error while opening output file \"" + fileName + "\": " + err);
	}

	csvFile << "SRC_IP,DST_IP,START_TIME,END_TIME,L3_PROTO,L4_PROTO,"
			   "SRC_PORT,DST_PORT,PACKETS,BYTES,PACKETS_REV,BYTES_REV\n";

	for (const FlowRecord& rec : _records) {
		// SRC_IP,DST_IP
		if (rec._l3Proto == L3Protocol::Ipv4) {
			csvFile << std::get<IPv4Address>(rec._fwdIpAddr).toString() << ",";
			csvFile << std::get<IPv4Address>(rec._revIpAddr).toString() << ",";
		} else if (rec._l3Proto == L3Protocol::Ipv6) {
			csvFile << std::get<IPv6Address>(rec._fwdIpAddr).toString() << ",";
			csvFile << std::get<IPv6Address>(rec._revIpAddr).toString() << ",";
		}

		// START_TIME
		uint64_t startUsec = TimevalToMicroseconds(rec._firstTs);
		uint64_t startMsec = startUsec / 1000;
		uint64_t startMsecDecimal = startUsec % 1000;
		csvFile << startMsec << "." << std::setfill('0') << std::setw(3) << startMsecDecimal << ",";
		// END_TIME
		uint64_t endUsec = TimevalToMicroseconds(rec._lastTs);
		uint64_t endMsec = endUsec / 1000;
		uint64_t endMsecDecimal = endUsec % 1000;
		csvFile << endMsec << "." << std::setfill('0') << std::setw(3) << endMsecDecimal << ",";
		// L3_PROTO
		csvFile << int(rec._l3Proto) << ",";
		// L4_PROTO
		csvFile << int(rec._l4Proto) << ",";
		// SRC_PORT
		csvFile << rec._fwdPort << ",";
		// DST_PORT
		csvFile << rec._revPort << ",";
		// PACKETS
		csvFile << rec._fwdPkts << ",";
		// BYTES
		csvFile << rec._fwdBytes << ",";
		// PACKETS_REV
		csvFile << rec._revPkts << ",";
		// BYTES_REV
		csvFile << rec._revBytes << "\n";
	}

	if (csvFile.fail()) {
		const std::string& err = errno != 0 ? std::strerror(errno) : "Unknown error";
		throw std::runtime_error("Error while writing to output file \"" + fileName + "\": " + err);
	}

	csvFile.close();
}

} // namespace generator
