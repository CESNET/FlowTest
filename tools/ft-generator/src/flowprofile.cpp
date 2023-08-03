/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Reading and representation of a flow profile
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flowprofile.h"
#include "config/common.h"

#include <pcap/pcap.h>
#include <pcapplusplus/EthLayer.h>

#include <cctype>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace generator {

// We expect the size of the generated flow to differ by at most this coefficient compared to the
// specified profile. This is used to calculate expected size of the generated flow on disk.
static constexpr double MAX_EXPECTED_SIZE_DEVIATION_COEF = 1.1;

using config::ParseValue;
using config::StringSplit;
using config::StringStrip;
using pcpp::IPv4Address;
using pcpp::IPv6Address;

/**
 * Attempt to convert the provided value to a L3Protocol value
 */
static std::optional<L3Protocol> GetL3Protocol(uint8_t value)
{
	switch (value) {
	case static_cast<uint8_t>(L3Protocol::Ipv4):
		return L3Protocol::Ipv4;
	case static_cast<uint8_t>(L3Protocol::Ipv6):
		return L3Protocol::Ipv6;
	default:
		return std::nullopt;
	}
}

std::string L3ProtocolToString(L3Protocol protocol)
{
	switch (protocol) {
	case L3Protocol::Unknown:
		return "Unknown";
	case L3Protocol::Ipv4:
		return "IPv4";
	case L3Protocol::Ipv6:
		return "IPv6";
	}

	return "<invalid>";
}

/**
 * Attempt to convert the provided value to a L4Protocol value
 */
static std::optional<L4Protocol> GetL4Protocol(uint8_t value)
{
	switch (value) {
	case static_cast<uint8_t>(L4Protocol::Icmp):
		return L4Protocol::Icmp;
	case static_cast<uint8_t>(L4Protocol::Tcp):
		return L4Protocol::Tcp;
	case static_cast<uint8_t>(L4Protocol::Udp):
		return L4Protocol::Udp;
	case static_cast<uint8_t>(L4Protocol::Icmpv6):
		return L4Protocol::Icmpv6;
	default:
		return std::nullopt;
	}
}

std::string L4ProtocolToString(L4Protocol protocol)
{
	switch (protocol) {
	case L4Protocol::Unknown:
		return "Unknown";
	case L4Protocol::Icmp:
		return "ICMP";
	case L4Protocol::Udp:
		return "UDP";
	case L4Protocol::Tcp:
		return "TCP";
	case L4Protocol::Icmpv6:
		return "ICMPv6";
	}

	return "<invalid>";
}

std::string FlowProfile::ToString() const
{
	std::stringstream ss;
	ss << "FlowProfile("
	   << "startTime=" << _startTime.ToMilliseconds() << ", "
	   << "endTime=" << _endTime.ToMilliseconds() << ", "
	   << "l3Proto=" << L3ProtocolToString(_l3Proto) << ", "
	   << "l4Proto=" << L4ProtocolToString(_l4Proto) << ", "
	   << "srcIp=" << (_srcIp ? _srcIp->toString() : "N/A") << ", "
	   << "dstIp=" << (_dstIp ? _dstIp->toString() : "N/A") << ", "
	   << "srcPort=" << _srcPort << ", "
	   << "dstPort=" << _dstPort << ", "
	   << "packets=" << _packets << ", "
	   << "bytes=" << _bytes << ", "
	   << "packetsRev=" << _packetsRev << ", "
	   << "bytesRev=" << _bytesRev << ")";
	return ss.str();
}

uint64_t FlowProfile::ExpectedSizeOnDisk() const
{
	uint64_t totalPktCount = _packets + _packetsRev;
	uint64_t extraPerPacket = sizeof(pcap_pkthdr) + sizeof(pcpp::ether_header);
	uint64_t totalByteCount = _bytes + _bytesRev;

	return uint64_t(totalByteCount * MAX_EXPECTED_SIZE_DEVIATION_COEF)
		+ totalPktCount * extraPerPacket;
}

FlowProfileReader::FlowProfileReader(const std::string& filename, bool skipUnknown)
	: _filename(filename)
	, _ifs(filename)
	, _skipUnknown(skipUnknown)
{
	if (!_ifs) {
		throw std::runtime_error("failed to open file \"" + filename + "\"");
	}

	ReadHeader();
}

void FlowProfileReader::Provide(std::vector<FlowProfile>& profiles)
{
	while (auto profile = ReadProfile()) {
		profiles.emplace_back(*profile);
	}

	_logger->info(
		"{} profile records loaded ({} records skipped)",
		_stats._parsed,
		_stats._skipped);
	_logger->info(
		"Profile summary: {} packets, {} bytes",
		_stats._parsedPackets,
		_stats._parsedBytes);
}

/**
 * @brief Get the next profile record from the file.
 * @return Profile or std::nullopt (no more records)
 */
std::optional<FlowProfile> FlowProfileReader::ReadProfile()
{
	while (std::optional<std::string> line = ReadLine()) {
		auto profile = ParseProfile(*line);
		if (!profile) {
			continue;
		}

		_stats._parsed++;
		_stats._parsedPackets += profile->_packets + profile->_packetsRev;
		_stats._parsedBytes += profile->_bytes + profile->_bytesRev;

		return profile;
	}

	return std::nullopt;
}

/**
 * @brief Parse a line and return the corresponding profile record.
 * @param line Line to parse.
 * @return Profile if valid. std::nullopt if line should be ignored.
 */
std::optional<FlowProfile> FlowProfileReader::ParseProfile(const std::string& line)
{
	FlowProfile profile;

	std::vector<std::string> pieces = StringSplit(line, ",");
	if (pieces.size() != _headerComponentsNum) {
		ThrowParseError(line, "bad line, invalid number of components");
	}

	for (auto& piece : pieces) {
		piece = StringStrip(piece);
	}

	std::optional<int64_t> startTime = ParseValue<int64_t>(pieces[_order[StartTime]]);
	if (!startTime) {
		ThrowParseError(line, "bad START_TIME");
	}
	profile._startTime = Timeval::FromMilliseconds(*startTime);

	std::optional<int64_t> endTime = ParseValue<int64_t>(pieces[_order[EndTime]]);
	if (!endTime) {
		ThrowParseError(line, "bad END_TIME");
	}
	profile._endTime = Timeval::FromMilliseconds(*endTime);

	std::optional<uint8_t> l3ProtoNum = ParseValue<uint8_t>(pieces[_order[L3Proto]]);
	if (!l3ProtoNum) {
		ThrowParseError(line, "bad L3_PROTO - invalid number");
	}
	std::optional<L3Protocol> l3Proto = GetL3Protocol(*l3ProtoNum);
	if (!l3Proto) {
		ThrowParseError(line, "bad L3_PROTO - unknown protocol");
	}
	profile._l3Proto = *l3Proto;

	std::optional<uint8_t> l4ProtoNum = ParseValue<uint8_t>(pieces[_order[L4Proto]]);
	if (!l4ProtoNum) {
		ThrowParseError(line, "bad L4_PROTO - invalid number");
	}
	std::optional<L4Protocol> l4Proto = GetL4Protocol(*l4ProtoNum);
	if (!l4Proto) {
		if (_skipUnknown) {
			_stats._skipped++;
			return std::nullopt;
		}

		ThrowParseError(line, "bad L4_PROTO - unknown protocol");
	}
	profile._l4Proto = *l4Proto;

	if (pieces[_order[SrcPort]].empty()) {
		profile._srcPort = 0;
	} else {
		std::optional<uint16_t> srcPort = ParseValue<uint16_t>(pieces[_order[SrcPort]]);
		if (!srcPort) {
			ThrowParseError(line, "bad SRC_PORT");
		}
		profile._srcPort = *srcPort;
	}

	if (pieces[_order[DstPort]].empty()) {
		profile._dstPort = 0;
	} else {
		std::optional<uint16_t> dstPort = ParseValue<uint16_t>(pieces[_order[DstPort]]);
		if (!dstPort) {
			ThrowParseError(line, "bad DST_PORT");
		}
		profile._dstPort = *dstPort;
	}

	std::optional<uint64_t> packets = ParseValue<uint64_t>(pieces[_order[Packets]]);
	if (!packets) {
		ThrowParseError(line, "bad PACKETS");
	}
	profile._packets = *packets;

	std::optional<uint64_t> bytes = ParseValue<uint64_t>(pieces[_order[Bytes]]);
	if (!bytes) {
		ThrowParseError(line, "bad BYTES");
	}
	profile._bytes = *bytes;

	std::optional<uint64_t> packetsRev = ParseValue<uint64_t>(pieces[_order[PacketsRev]]);
	if (!packetsRev) {
		ThrowParseError(line, "bad PACKETS_REV");
	}
	profile._packetsRev = *packetsRev;

	std::optional<uint64_t> bytesRev = ParseValue<uint64_t>(pieces[_order[BytesRev]]);
	if (!bytesRev) {
		ThrowParseError(line, "bad BYTES_REV");
	}
	profile._bytesRev = *bytesRev;

	if (_order[SrcIp] != -1) {
		const auto& srcIpStr = pieces[_order[SrcIp]];
		// Allow not specifying the IP address by leaving the value of the field blank
		if (!srcIpStr.empty()) {
			if (profile._l3Proto == L3Protocol::Ipv4) {
				IPv4Address ipv4(srcIpStr);
				if (!ipv4.isValid()) {
					ThrowParseError(line, "bad SRC_IP - invalid ipv4 address");
				}
				profile._srcIp = ipv4;
			} else if (profile._l3Proto == L3Protocol::Ipv6) {
				IPv6Address ipv6(srcIpStr);
				if (!ipv6.isValid()) {
					ThrowParseError(line, "bad SRC_IP - invalid ipv6 address");
				}
				profile._srcIp = ipv6;
			} else {
				ThrowParseError(line, "bad SRC_IP - invalid L3 protocol for address");
			}
		}
	}

	if (_order[DstIp] != -1) {
		const auto& dstIpStr = pieces[_order[DstIp]];
		// Allow not specifying the IP address by leaving the value of the field blank
		if (!dstIpStr.empty()) {
			if (profile._l3Proto == L3Protocol::Ipv4) {
				IPv4Address ipv4(dstIpStr);
				if (!ipv4.isValid()) {
					ThrowParseError(line, "bad DST_IP - invalid ipv4 address");
				}
				profile._dstIp = ipv4;
			} else if (profile._l3Proto == L3Protocol::Ipv6) {
				IPv6Address ipv6(dstIpStr);
				if (!ipv6.isValid()) {
					ThrowParseError(line, "bad DST_IP - invalid ipv6 address");
				}
				profile._dstIp = ipv6;
			} else {
				ThrowParseError(line, "bad DST_IP - invalid L3 protocol for address");
			}
		}
	}

	if (profile._startTime > profile._endTime) {
		ThrowParseError(line, "bad START_TIME > END_TIME");
	}

	return profile;
}

/**
 * @brief Read a line from the file while skipping empty and commented out lines
 *
 * @return The read line if there is any left, std::nullopt if end of file was reached
 */
std::optional<std::string> FlowProfileReader::ReadLine()
{
	std::string line;

	while (std::getline(_ifs, line)) {
		_lineNum++;

		line = StringStrip(line);
		if (line.empty() || line[0] == '#') {
			continue;
		}

		return line;
	}

	if (_ifs.eof()) {
		return std::nullopt;
	}

	throw std::runtime_error("failed to read file");
}

/**
 * @brief Process the header to figure out the order of components
 */
void FlowProfileReader::ReadHeader()
{
	auto line = ReadLine();
	if (!line) {
		throw std::runtime_error("failed to read header - reached end of file");
	}

	auto pieces = StringSplit(*line, ",");

	for (auto& piece : pieces) {
		piece = StringStrip(piece);
	}

	const std::unordered_map<std::string, Component> componentMap {
		{"START_TIME", StartTime},
		{"END_TIME", EndTime},
		{"L3_PROTO", L3Proto},
		{"L4_PROTO", L4Proto},
		{"SRC_PORT", SrcPort},
		{"DST_PORT", DstPort},
		{"PACKETS", Packets},
		{"BYTES", Bytes},
		{"PACKETS_REV", PacketsRev},
		{"BYTES_REV", BytesRev},
		{"SRC_IP", SrcIp},
		{"DST_IP", DstIp},
	};

	_order.fill(-1);

	for (std::size_t i = 0; i < pieces.size(); i++) {
		auto it = componentMap.find(pieces[i]);
		if (it == componentMap.end()) {
			throw std::runtime_error("invalid component in header \"" + pieces[i] + "\"");
		}

		if (_order[it->second] != -1) {
			throw std::runtime_error("component in header \"" + pieces[i] + "\" appeared twice");
		}

		_order[it->second] = i;
	}

	for (const auto& [cName, cId] : componentMap) {
		if (_order[cId] != -1) {
			_headerComponentsNum++;
			continue;
		}

		if (cName != "SRC_IP" && cName != "DST_IP") {
			throw std::runtime_error("component \"" + cName + "\" missing in header");
		}
	}
}

/**
 * @brief Triggered on a parse error
 *
 * @param value         The errorneous value
 * @param errorMessage  The error message
 *
 * @throw runtime_error is always thrown
 */
[[noreturn]] void
FlowProfileReader::ThrowParseError(const std::string& value, const std::string& errorMessage)
{
	throw std::runtime_error(
		"Invalid line in profile \"" + value + "\": " + errorMessage + " (" + _filename + ":"
		+ std::to_string(_lineNum) + ")");
}

} // namespace generator
