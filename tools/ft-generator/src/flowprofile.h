/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Reading and representation of a flow profile
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <array>
#include <fstream>
#include <optional>
#include <string>

namespace generator {

enum class L3Protocol : uint8_t {
	Unknown,
	Ipv4 = 4,
	Ipv6 = 6
};

enum class L4Protocol : uint8_t {
	Unknown,
	Icmp = 1,
	Tcp = 6,
	Udp = 17,
	Icmpv6 = 58
};


/**
 * @brief Struct representing a flow profile entry in the input file
 */
struct FlowProfile {
	timeval _startTime;
	timeval _endTime;
	L3Protocol _l3Proto;
	L4Protocol _l4Proto;
	uint16_t _srcPort;
	uint16_t _dstPort;
	uint64_t _packets;
	uint64_t _bytes;
	uint64_t _packetsRev;
	uint64_t _bytesRev;

	std::string ToString() const;
};

/**
 * @brief An interface of a flow profile provider
 */
class FlowProfileProvider {
public:
	/**
	 * @brief Provide flow profiles to the supplied vector
	 *
	 * @param profiles  The vector to fill with the provided flow profiles
	 */
	virtual void Provide(std::vector<FlowProfile>& profiles) = 0;
};

/**
 * @brief A reader of flow profile entries from an input file
 */
class FlowProfileReader : public FlowProfileProvider {
public:
	/**
	 * @brief Construct a new Flow Profile Reader object
	 *
	 * @param filename  The filename
	 *
	 * @throws std::runtime_error  When the provided file couldn't be read or has invalid header
	 */
	explicit FlowProfileReader(const std::string& filename);

	/**
	 * @brief Provide flow profiles to the supplied vector
	 *
	 * While reading the file invalid lines are skipped and an error message is printed
	 *
	 * @param profiles  The vector to fill with the provided flow profiles
	 *
	 * @throws std::runtime_error  When error happened while reading the file
	 */
	void Provide(std::vector<FlowProfile>& profiles) override;

private:
	enum Component {
		StartTime = 0,
		EndTime,
		L3Proto,
		L4Proto,
		SrcPort,
		DstPort,
		Packets,
		Bytes,
		PacketsRev,
		BytesRev,
		ComponentsCount
	};

	std::array<int, ComponentsCount> _order;
	std::string _filename;
	std::ifstream _ifs;
	unsigned int _lineNum = 0;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("FlowProfileReader");

	std::optional<std::string> ReadLine();
	void ReadHeader();
	std::optional<FlowProfile> ReadProfile();
	void ReportParseError(const std::string& value, const std::string& errorMessage);
};

} // namespace generator
