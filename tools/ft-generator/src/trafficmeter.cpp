/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Traffic meter
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "trafficmeter.h"

#include <arpa/inet.h>
#include <pcapplusplus/PayloadLayer.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>

#include <cstring>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace generator {

static bool ExtractUdpParamsFromPayloadLayer(
	const pcpp::PayloadLayer* payloadLayer,
	uint16_t& srcPort,
	uint16_t& dstPort)
{
	if (payloadLayer->getDataLen() < sizeof(pcpp::udphdr)) {
		return false;
	}

	const pcpp::udphdr* udpHdr = reinterpret_cast<const pcpp::udphdr*>(payloadLayer->getDataPtr());
	srcPort = ntohs(udpHdr->portSrc);
	dstPort = ntohs(udpHdr->portDst);

	return true;
}

static bool ExtractTcpParamsFromPayloadLayer(
	const pcpp::PayloadLayer* payloadLayer,
	uint16_t& srcPort,
	uint16_t& dstPort)
{
	if (payloadLayer == nullptr || payloadLayer->getDataLen() < sizeof(pcpp::tcphdr)) {
		return false;
	}

	const pcpp::tcphdr* tcpHdr = reinterpret_cast<const pcpp::tcphdr*>(payloadLayer->getDataPtr());
	srcPort = ntohs(tcpHdr->portSrc);
	dstPort = ntohs(tcpHdr->portDst);

	return true;
}

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

	// Extract source and destination ports from the TCP/UDP layer of the generated packet
	srcPort = 0;
	dstPort = 0;

	if (l4Proto == L4Protocol::Tcp) {
		pcpp::TcpLayer* tcpLayer = static_cast<pcpp::TcpLayer*>(packet.getLayerOfType(pcpp::TCP));
		if (tcpLayer != nullptr) {
			srcPort = tcpLayer->getSrcPort();
			dstPort = tcpLayer->getDstPort();
		} else {
			// In case of a fragmented packet, the TCP/UDP layer might become a generic Payload
			// layer, try to parse the parameters from that instead
			pcpp::PayloadLayer* payloadLayer
				= dynamic_cast<pcpp::PayloadLayer*>(packet.getLastLayer());
			if (!ExtractTcpParamsFromPayloadLayer(payloadLayer, srcPort, dstPort)) {
				// If this happens, it is probably a bug. Should we crash instead?
				_logger->error(
					"Could not parse TCP ports from generated packet, but the specified L4 in flow "
					"profile is TCP. Generated report will contain 0 as the port number.");
			}
		}

	} else if (l4Proto == L4Protocol::Udp) {
		pcpp::UdpLayer* udpLayer = static_cast<pcpp::UdpLayer*>(packet.getLayerOfType(pcpp::UDP));
		if (udpLayer != nullptr) {
			srcPort = udpLayer->getSrcPort();
			dstPort = udpLayer->getDstPort();
		} else {
			// In case of a fragmented packet, the TCP/UDP layer might become a generic Payload
			// layer, try to parse the parameters from that instead
			pcpp::PayloadLayer* payloadLayer
				= dynamic_cast<pcpp::PayloadLayer*>(packet.getLastLayer());
			if (!ExtractUdpParamsFromPayloadLayer(payloadLayer, srcPort, dstPort)) {
				// If this happens, it is probably a bug. Should we crash instead?
				_logger->error(
					"Could not parse UDP ports from generated packet, but the specified L4 in flow "
					"profile is UDP. Generated report will contain 0 as the port number.");
			}
		}
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
	Timeval time,
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
		uint64_t startUsec = rec._firstTs.ToMicroseconds();
		uint64_t startMsec = startUsec / 1000;
		uint64_t startMsecDecimal = startUsec % 1000;
		csvFile << startMsec << "." << std::setfill('0') << std::setw(3) << startMsecDecimal << ",";
		// END_TIME
		uint64_t endUsec = rec._lastTs.ToMicroseconds();
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
