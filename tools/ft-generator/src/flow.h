/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "addressgenerators.h"
#include "flowprofile.h"
#include "packet.h"

#include <pcapplusplus/Packet.h>

#include <iostream>
#include <memory>
#include <vector>
#include <list>

namespace generator {

class Layer;

/**
 * @brief Extra information about the flow packet
 */
struct PacketExtraInfo {
	int64_t _time;         //< The timestamp of the packet
	Direction _direction;  //< The direction of the packet
};

/**
 * @brief Class representing the flow interface
 */
class Flow {
public:
	uint64_t _fwdPackets = 0;   //< Number of packets in forward direction
	uint64_t _revPackets = 0;   //< Number of packets in reverse direction
	uint64_t _fwdBytes = 0;     //< Number of bytes in forward direction
	uint64_t _revBytes = 0;     //< Number of bytes in reverse direction
	uint64_t _tsFirst = 0;      //< Timestamp of the first packet
	uint64_t _tsLast = 0;       //< Timestamp of the last packet
	std::list<Packet> _packets; //< The planned packets

	/**
	 * @brief Construct a new Flow object
	 *
	 * @param id  The ID of the flow
	 *
	 * @throws std::runtime_error  When the provided flow profile has invalid values
	 */
	Flow(uint64_t id, const FlowProfile& profile, AddressGenerators& addressGenerators);

	/**
	 * @brief Destroy the Flow object
	 *
	 */
	~Flow();

	/**
	 * @brief Get an ID of the flow
	 *
	 * @return The ID
	 */
	uint64_t GetId() const { return _id; }

	/**
	 * @brief Generate a next packet
	 *
	 * @param packet  The packet object to generate to
	 *
	 * @throws std::runtime_error  If the flow is already finished
	 */
	std::pair<pcpp::Packet, PacketExtraInfo> GenerateNextPacket();

	/**
	 * @brief Get the time of next packet
	 *
	 * @return int64_t  The time in seconds
	 */
	int64_t GetNextPacketTime() const;

	/**
	 * @brief Check whether the flow is finished and wont be generating any additional packets
	 *
	 * @return true or false
	 */
	bool IsFinished() const;

private:
	friend class PacketFlowSpan;

	uint64_t _id;
	std::vector<std::unique_ptr<Layer>> _layerStack;

	/**
	 * @brief Add new protocol layer.
	 *
	 * @note Layer has to be added in order
	 *
	 * @param layer Pointer to layer
	 */
	void AddLayer(std::unique_ptr<Layer> layer);

	/**
	 * @brief Plan all packets in flow.
	 */
	void Plan();

	/**
	 * @brief Plan direction to packets with UNDEFINED direction.
	 */
	void PlanPacketsDirections();

	/**
	 * @brief Plan packets timestamps.
	 *
	 * First and last packet are assigned timestamps _tsFirst and _tsLast.
	 * Other packets have timestamps in this range with randomly generated uniform distribution.
	 */
	void PlanPacketsTimestamps();

	/**
	 * @brief Plan packets sizes.
	 */
	void PlanPacketsSizes();
};

} // namespace generator
