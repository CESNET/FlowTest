/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Reading and representation of a flow profile
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "timeval.h"

#include <pcapplusplus/IpAddress.h>
#include <sys/time.h>

#include <array>
#include <fstream>
#include <optional>
#include <string>

namespace generator {

using pcpp::IPAddress;

/**
 * @brief Enumeration of possible L3 protocols
 */
enum class L3Protocol : uint8_t { Unknown, Ipv4 = 4, Ipv6 = 6 };

/**
 * @brief Convert a L3Protocol value to a string representation
 */
std::string L3ProtocolToString(L3Protocol protocol);

/**
 * @brief Enumeration of possible L4 protocols
 */
enum class L4Protocol : uint8_t { Unknown, Icmp = 1, Tcp = 6, Udp = 17, Icmpv6 = 58 };

/**
 * @brief Convert a L4Protocol value to a string representation
 */
std::string L4ProtocolToString(L4Protocol protocol);

/**
 * @brief Struct representing a flow profile entry in the input file
 */
struct FlowProfile {
	Timeval _startTime;
	Timeval _endTime;
	L3Protocol _l3Proto;
	L4Protocol _l4Proto;
	uint16_t _srcPort;
	uint16_t _dstPort;
	uint64_t _packets;
	uint64_t _bytes;
	uint64_t _packetsRev;
	uint64_t _bytesRev;
	std::optional<IPAddress> _srcIp;
	std::optional<IPAddress> _dstIp;

	/**
	 * @brief Get a string representation of a FlowProfile
	 */
	std::string ToString() const;

	/**
	 * @brief The expected size the generated flow will occupy on disk
	 *
	 * This includes the total number of bytes in each direction as well as the PCAP
	 * packet header (as the packet will be saved into a PCAP file) + ethernet
	 * header length (not accounted for in the flow profile) for each packet.
	 *
	 * The size is slightly overestimated to account for additional necessary
	 * packets that might be injected into the flow. This is controlled by the
	 * MAX_EXPECTED_SIZE_DEVIATION_COEF constant.
	 */
	uint64_t ExpectedSizeOnDisk() const;
};

/**
 * @brief Structure with statistics about profile records in a file
 */
struct FlowProfileStats {
	/** Successfully parsed profile records */
	uint64_t _parsed = 0;
	/** Skipped profile records e.g. due to unsupported L4 protocol */
	uint64_t _skipped = 0;

	/** Sum of packets in successfully parsed records */
	uint64_t _parsedPackets = 0;
	/** Sum of bytes in successfully parsed records */
	uint64_t _parsedBytes = 0;
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
	 * @param skipUnknown Skip/ignore records with unknown/unsupported protocols
	 *
	 * @throws std::runtime_error  When the provided file couldn't be read or has invalid header
	 */
	explicit FlowProfileReader(const std::string& filename, bool skipUnknown = false);

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
		SrcIp,
		DstIp,
		ComponentsCount
	};

	std::array<int, ComponentsCount> _order;
	std::string _filename;
	std::ifstream _ifs;
	unsigned int _lineNum = 0;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("FlowProfileReader");
	unsigned int _headerComponentsNum = 0;

	bool _skipUnknown;
	FlowProfileStats _stats;

	std::optional<std::string> ReadLine();
	void ReadHeader();

	std::optional<FlowProfile> ParseProfile(const std::string& line);
	std::optional<FlowProfile> ReadProfile();

	[[noreturn]] void ThrowParseError(const std::string& value, const std::string& errorMessage);
};

} // namespace generator
