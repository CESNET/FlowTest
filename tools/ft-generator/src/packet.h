/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <ctime>
#include <map>
#include <variant>
#include <vector>
#include <utility>
#include <iostream>

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


/**
 * @brief Class representing the packet planner interface
 */
class Packet {
public:
	using layerValue  = std::variant<std::monostate, uint64_t>;
	using layerParams = std::map<int, layerValue>;
	using layer       = std::pair<Layer *, layerParams>;

	Direction _direction = Direction::Unknown;    //< Packet direction
	timeval _timestamp = {0, 0};                  //< Timestamp
	size_t _size = 0;                             //< Planned packet size (IP header and above)
	bool _isFinished = false;                     //< Do not add more layers to packet
	std::vector<layer> _layers;                   //< Packet protocol layers in order
	bool _isExtra = false;                        //< Mark the packet as extra
};

} // namespace generator
