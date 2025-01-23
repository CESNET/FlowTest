/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Class representing network biflow object as contained in the network profile.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "common.hpp"
#include "timestamp.h"

/**
 * @brief Structure representing a biflow record present in the profile.
 */
struct __attribute__((aligned(64))) Biflow {
	/**
	 * @brief Initialize Biflow object from a single biflow record in the profile.
	 *
	 * Expected format:
	 * START_TIME,END_TIME,L3_PROTO,L4_PROTO,SRC_PORT,DST_PORT,PACKETS,BYTES,PACKETS_REV,BYTES_REV
	 *
	 * @param record biflow record in the expected format
	 * @throws std::runtime_error parsing errors of individual record fields
	 */
	explicit Biflow(std::string_view record);

	/**
	 * @brief Parse a flow field out of a CSV line.
	 * @tparam T type of the value
	 * @param line beginning of the line
	 * @param value argument where the parsed value should be stored
	 * @return rest of the line after parsing the field
	 * @throws runtime_error parsing error
	 */
	template <typename T>
	std::string_view ConsumeField(std::string_view line, T& value);
	friend std::ostream& operator<<(std::ostream& os, const Biflow& f);
	/**
	 * @brief Get number of packets belonging to the interval.
	 * 	Packets are averaged – may be real number.
	 * @param start Start time of the interval.
	 * @param end End time of the interval.
	 * @return Number of packets belonging to the interval (may be real number).
	 */
	double PacketsInInterval(ft::Timestamp start, ft::Timestamp end) const;
	/**
	 * @brief Get number of bytes belonging to the interval.
	 * 	Bytes are estimated based on the average packet size.
	 * @param start Start time of the interval.
	 * @param end End time of the interval.
	 * @return Number of bytes belonging to the interval (may be real number).
	 */
	double BytesInInterval(ft::Timestamp start, ft::Timestamp end) const;

	ft::Timestamp start_time {};
	ft::Timestamp end_time {};
	uint64_t packets {};
	uint64_t bytes {};
	uint64_t packets_rev {};
	uint64_t bytes_rev {};
	uint16_t src_port {};
	uint16_t dst_port {};
	uint16_t l4_proto {};
	uint8_t l3_proto {};

	/** Duration of flow precomputed for efficiency (end_time - start_time). */
	ft::Timestamp duration;
	/** Packets in both directions precomputed for efficiency (packets + packets_rev). */
	uint64_t packets_total;
	/** How many bytes per packet used to compute bytes per second/window
	 * ((bytes + bytes_rev) / packets_total). */
	double bytes_per_packet;
};
