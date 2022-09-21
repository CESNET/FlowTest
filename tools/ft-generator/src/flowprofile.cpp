/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Reading and representation of a flow profile
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flowprofile.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace generator {

static timeval MillisecsToTimeval(int64_t millisecs)
{
	timeval time;
	time.tv_sec = millisecs / 1000;
	time.tv_usec = (millisecs % 1000) * 1000;
	return time;
}

static int64_t TimevalToMilliseconds(const timeval& time)
{
	return (int64_t(time.tv_sec) * 1000) + (time.tv_usec / 1000);
}

/**
 * Split a string by a delimiter and return the pieces
 */
static std::vector<std::string> stringSplit(const std::string& s, const std::string& delimiter)
{
	std::size_t pos = 0;
	std::size_t nextPos;
	std::vector<std::string> pieces;
	while ((nextPos = s.find(delimiter, pos)) != std::string::npos) {
		pieces.emplace_back(s.begin() + pos, s.begin() + nextPos);
		pos = nextPos + delimiter.size();
	}
	pieces.emplace_back(s.begin() + pos, s.end());
	return pieces;
}

/**
 * Remove whitespace from the beginning and end of a string
 */
static std::string stringStrip(std::string s)
{
	auto isNotWhitespace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), isNotWhitespace));
	s.erase(std::find_if(s.rbegin(), s.rend(), isNotWhitespace).base(), s.end());
	return s;
}

/**
 * Attempt to parse a value of the provided type
 */
template <typename T>
static std::optional<T> parseValue(const std::string& s)
{
	T value;
	auto [endPtr, errCode] = std::from_chars(s.data(), s.data() + s.size(), value);
	if (errCode == std::errc() && endPtr == s.data() + s.size()) {
		return value;
	} else {
		return std::nullopt;
	}
}

/**
 * Attempt to convert the provided value to a L3Protocol value
 */
static std::optional<L3Protocol> getL3Protocol(uint8_t value)
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

/**
 * Convert a L3Protocol value to a string representation
 */
static std::string l3ProtocolToString(L3Protocol protocol)
{
	switch (protocol) {
	case L3Protocol::Unknown:
		return "Unknown";
	case L3Protocol::Ipv4:
		return "Ipv4";
	case L3Protocol::Ipv6:
		return "Ipv6";
	}

	return "<invalid>";
}

/**
 * Attempt to convert the provided value to a L4Protocol value
 */
static std::optional<L4Protocol> getL4Protocol(uint8_t value)
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

/**
 * Convert a L4Protocol value to a string representation
 */
static std::string l4ProtocolToString(L4Protocol protocol)
{
	switch (protocol) {
	case L4Protocol::Unknown:
		return "Unknown";
	case L4Protocol::Icmp:
		return "Icmp";
	case L4Protocol::Udp:
		return "Udp";
	case L4Protocol::Tcp:
		return "Tcp";
	case L4Protocol::Icmpv6:
		return "Icmpv6";
	}

	return "<invalid>";
}

/**
 * Get a string representation of a FlowProfile
 */
std::string FlowProfile::ToString() const
{
	std::stringstream ss;
	ss << "FlowProfile("
		<< "startTime=" << TimevalToMilliseconds(_startTime) << ", "
		<< "endTime=" << TimevalToMilliseconds(_endTime) << ", "
		<< "l3Proto=" << l3ProtocolToString(_l3Proto) << ", "
		<< "l4Proto=" << l4ProtocolToString(_l4Proto) << ", "
		<< "srcPort=" << _srcPort << ", "
		<< "dstPort=" << _dstPort << ", "
		<< "packets=" << _packets << ", "
		<< "bytes=" << _bytes << ", "
		<< "packetsRev=" << _packetsRev << ", "
		<< "bytesRev=" << _bytesRev << ")";
	return ss.str();
}

FlowProfileReader::FlowProfileReader(const std::string& filename)
	: _filename(filename), _ifs(filename)
{
	if (!_ifs) {
		throw std::runtime_error("failed to open file \"" + filename + "\"");
	}

	ReadHeader();
}

void FlowProfileReader::Provide(std::vector<FlowProfile>& profiles)
{
	while (auto profile = ReadProfile()) {
		profiles.emplace_back(std::move(*profile));
	}
}

/**
 * @brief Parse and return the next FlowProfile if available
 *
 * Invalid lines are skipped and an error message is printed
 *
 * @return The FlowProfile if there is any left, else std::nullopt
 */
std::optional<FlowProfile> FlowProfileReader::ReadProfile()
{
	FlowProfile profile;

	auto maybeLine = ReadLine();
	if (!maybeLine) {
		return std::nullopt;
	}

	const std::string& line = *maybeLine;

	std::vector<std::string> pieces = stringSplit(line, ",");
	if (pieces.size() != ComponentsCount) {
		ReportParseError(line, "bad line, invalid number of components");
		return ReadProfile();
	}

	std::optional<int64_t> startTime = parseValue<int64_t>(pieces[_order[StartTime]]);
	if (!startTime) {
		ReportParseError(line, "bad START_TIME");
		return ReadProfile();
	}
	profile._startTime = MillisecsToTimeval(*startTime);

	std::optional<int64_t> endTime = parseValue<int64_t>(pieces[_order[EndTime]]);;
	if (!endTime) {
		ReportParseError(line, "bad END_TIME");
		return ReadProfile();
	}
	profile._endTime = MillisecsToTimeval(*endTime);

	std::optional<uint8_t> l3ProtoNum = parseValue<uint8_t>(pieces[_order[L3Proto]]);
	if (!l3ProtoNum) {
		ReportParseError(line, "bad L3_PROTO - invalid number");
		return ReadProfile();
	}
	std::optional<L3Protocol> l3Proto = getL3Protocol(*l3ProtoNum);
	if (!l3Proto) {
		ReportParseError(line, "bad L3_PROTO - unknown protocol");
		return ReadProfile();
	}
	profile._l3Proto = *l3Proto;

	std::optional<uint8_t> l4ProtoNum = parseValue<uint8_t>(pieces[_order[L4Proto]]);
	if (!l4ProtoNum) {
		ReportParseError(line, "bad L4_PROTO - invalid number");
		return ReadProfile();
	}
	std::optional<L4Protocol> l4Proto = getL4Protocol(*l4ProtoNum);
	if (!l4Proto) {
		ReportParseError(line, "bad L4_PROTO - unknown protocol");
		return ReadProfile();
	}
	profile._l4Proto = *l4Proto;

	if (pieces[_order[SrcPort]].empty()) {
		profile._srcPort = 0;
	} else {
		std::optional<uint16_t> srcPort = parseValue<uint16_t>(pieces[_order[SrcPort]]);
		if (!srcPort) {
			ReportParseError(line, "bad SRC_PORT");
			return ReadProfile();
		}
		profile._srcPort = *srcPort;
	}

	if (pieces[_order[DstPort]].empty()) {
		profile._dstPort = 0;
	} else {
		std::optional<uint16_t> dstPort = parseValue<uint16_t>(pieces[_order[DstPort]]);
		if (!dstPort) {
			ReportParseError(line, "bad DST_PORT");
			return ReadProfile();
		}
		profile._dstPort = *dstPort;
	}

	std::optional<uint64_t> packets = parseValue<uint64_t>(pieces[_order[Packets]]);
	if (!packets) {
		ReportParseError(line, "bad PACKETS");
		return ReadProfile();
	}
	profile._packets = *packets;

	std::optional<uint64_t> bytes = parseValue<uint64_t>(pieces[_order[Bytes]]);
	if (!bytes) {
		ReportParseError(line, "bad BYTES");
		return ReadProfile();
	}
	profile._bytes = *bytes;

	std::optional<uint64_t> packetsRev = parseValue<uint64_t>(pieces[_order[PacketsRev]]);
	if (!packetsRev) {
		ReportParseError(line, "bad PACKETS_REV");
		return ReadProfile();
	}
	profile._packetsRev = *packetsRev;

	std::optional<uint64_t> bytesRev = parseValue<uint64_t>(pieces[_order[BytesRev]]);
	if (!bytesRev) {
		ReportParseError(line, "bad BYTES_REV");
		return ReadProfile();
	}
	profile._bytesRev = *bytesRev;

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
	if (!std::getline(_ifs, line)) {
		if (!_ifs.eof()) {
			throw std::runtime_error("failed to read file");
		}
		return std::nullopt;
	}
	_lineNum++;

	line = stringStrip(line);
	if (line.empty() || line[0] == '#') {
		return ReadLine();
	}

	return line;
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

	auto pieces = stringSplit(*line, ",");
	if (pieces.size() != ComponentsCount) {
		throw std::runtime_error("invalid number of components in header");
	}

	const std::unordered_map<std::string, Component> componentMap{
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
		if (_order[cId] == -1) {
			throw std::runtime_error("component \"" + cName + "\" missing in header");
		}
	}
}

/**
 * @brief Triggered on a parse error
 *
 * @param value         The errorneous value
 * @param errorMessage  The error message
 */
void FlowProfileReader::ReportParseError(const std::string& value, const std::string& errorMessage)
{
	_logger->error("Ignoring line with invalid value \"{}\": {} ({}:{})",
		value,
		errorMessage,
		_filename,
		_lineNum);
}

} // namespace generator
