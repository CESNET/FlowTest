/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "timeval.h"

#include <cassert>
#include <ctime>
#include <iostream>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace generator {

class Layer;

/**
 * @brief Packet direction
 *
 * @note Completed packet has to contains FORWARD or REVERSE direction.
 */
enum class Direction {
	Unknown,
	Forward,
	Reverse,
};

static inline Direction SwapDirection(Direction dir)
{
	assert(dir == Direction::Forward || dir == Direction::Reverse);
	return dir == Direction::Forward ? Direction::Reverse : Direction::Forward;
}

/**
 * @brief Class representing the packet planner interface
 */
class Packet {
public:
	using layerValue = std::variant<std::monostate, uint64_t>;
	using layerParams = std::map<int, layerValue>;
	using layer = std::pair<Layer*, layerParams>;

	Direction _direction = Direction::Unknown; //< Packet direction
	Timeval _timestamp; //< Timestamp
	size_t _size = 0; //< Planned packet size (IP header and above)
	bool _isFinished = false; //< Do not add more layers to packet
	std::vector<layer> _layers; //< Packet protocol layers in order
	bool _isExtra = false; //< Mark the packet as extra
};

} // namespace generator
