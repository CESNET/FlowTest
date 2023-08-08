/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow plan helper
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "flow.h"
#include "packet.h"

#include <cstdint>
#include <random>

namespace generator {

/**
 * @brief A helper class used in layers in the planning phase
 *
 * Mainly used for tracking remaining flow resources and providing useful utility functions for
 * planning
 */
class FlowPlanHelper {
public:
	Flow* _flow; //< The flow
	Packet* _packet = nullptr; //< The current packet
	uint64_t _assignedFwdPkts; //< Number of assigned packets in the forward direction
	uint64_t _assignedRevPkts; //< Number of assigned packets in the reverse direction
	uint64_t _assignedFwdBytes; //< Number of assigned bytes in the forward direction
	uint64_t _assignedRevBytes; //< Number of assigned bytes in the forward direction

	/**
	 * @brief Construct a new Flow Plan Helper object
	 *
	 * @param flow
	 */
	explicit FlowPlanHelper(Flow& flow);

	/**
	 * @brief Move to the next available packet
	 *
	 * A packet is considered "available" if it has the finished flag unset
	 *
	 * @return Pointer to the packet
	 */
	Packet* NextPacket();

	/**
	 * @brief Get the number of packets that have been traversed through from
	 *        the start. This does not include the current packet.
	 */
	uint64_t PktsFromStart();

	/**
	 * @brief Get the number of packets till the iterator reaches end. In other
	 *        words, how many times NextPacket() will return another packet.
	 */
	uint64_t PktsTillEnd();

	/**
	 * @brief Get the number of remaining unassigned packets
	 *
	 * @return uint64_t
	 */
	uint64_t PktsRemaining();

	/**
	 * @brief Get the number of remaining packets in the specified direction
	 *
	 * @param dir  The direction
	 */
	uint64_t PktsRemaining(Direction dir);

	/**
	 * @brief Get the number of remaining unassigned packets in the forward direction
	 *
	 * @return uint64_t
	 */
	uint64_t FwdPktsRemaining();

	/**
	 * @brief Get the number of remaining unassigned packets in the reverse direction
	 *
	 * @return uint64_t
	 */
	uint64_t RevPktsRemaining();

	/**
	 * @brief Get the number of remaining unassigned bytes
	 *
	 * @return uint64_t
	 */
	uint64_t BytesRemaining();

	/**
	 * @brief Get the number of remaining bytes in the specified direction
	 *
	 * @param dir  The direction
	 */
	uint64_t BytesRemaining(Direction dir);

	/**
	 * @brief Get the number of remaining unassigned bytes in the forward direction
	 *
	 * @return uint64_t
	 */
	uint64_t FwdBytesRemaining();

	/**
	 * @brief Get the number of remaining unassigned bytes in the reverse direction
	 *
	 * @return uint64_t
	 */
	uint64_t RevBytesRemaining();

	/**
	 * @brief Generate a random available direction for a packet
	 *
	 * @return The direction
	 */
	Direction GetRandomDir();

	/**
	 * @brief Reset the flow packet iterator to the beginning of the flow. Does not reset the
	 * counters!
	 * @return uint64_t
	 */
	void Reset();

	/**
	 * @brief Includes a packet in the assigned counts
	 *
	 * @param pkt  The packet
	 */
	void IncludePkt(Packet* pkt);

private:
	uint64_t _nUnfinished = 0;
	uint64_t _nTraversed = 0;
	uint64_t _nUnfinishedTraversed = 0;
	std::list<Packet>::iterator _it;
	double _fwdPktChance;
};

} // namespace generator
