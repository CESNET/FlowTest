/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Timestamp generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config/timestamps.h"
#include "packet.h"
#include "timestamp.h"

#include <list>

namespace generator {

/**
 * @brief Internal struct of TimestampGenerator
 */
struct Gap {
	std::size_t index;
	uint64_t valueMin;
	uint64_t value;
};

/**
 * @brief Calculate the minimal time gap following a packet
 *
 * @param packetSize  Size of the packet (from and including L2 headers) that the gap will follow
 *
 * @return The minimal time gap in picoseconds
 */
uint64_t CalcMinGlobalPacketGapPicos(uint64_t packetSize, const config::Timestamps& config);

/**
 * @brief Packet timestamp generator class
 */
class TimestampGenerator {
public:
	/**
	 * @brief Generate packet timestamps
	 *
	 * @param packets  The list of packets
	 * @param tsFirst  Timestamp of the first packet
	 * @param tsLast  Timestamp of the last packet
	 * @param config  The configuration section for timestamp generation
	 * @param sizeTillIpLayer  The number of bytes of a packet up to but not including the IP layer.
	 * @return A vector of timestamps in microseconds
	 *
	 * @throws std::logic_error if invalid information about the first and last timestamp is
	 * provided
	 * @throws std::overflow_error if conversion of flow duration to picoseconds would overflow
	 * uint64
	 *
	 * If the maximum gap limit cannot be satisfied with the parameters provided, the flow is
	 * trimmed such that the number of packets and first timestamp stays the same, but the last
	 * timestamp is moved closer to start.
	 */
	TimestampGenerator(
		const std::list<Packet>& packets,
		const ft::Timestamp& tsFirst,
		const ft::Timestamp& tsLast,
		const config::Timestamps& config,
		uint64_t sizeTillIpLayer);

	/**
	 * @brief Get the next timestamp. This operation does not consume the timestamp.
	 */
	ft::Timestamp Get() const;

	/**
	 * @brief Consume the current timestamp and move onto the next.
	 */
	void Next();

	/**
	 * @brief Shift the current timestamp and modify the subsequent timestamps to accomodate for the
	 * shift
	 *
	 * @param nanosecs  The amount of nanoseconds to shift by
	 */
	void Shift(uint64_t nanosecs);

private:
	ft::Timestamp _tsFirst;
	ft::Timestamp _tsLast;

	std::vector<Gap> _gaps;
	std::size_t _gapIdx = 0;
	uint64_t _accumGap = 0;
};

} // namespace generator
