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
 *
 * @note Still a work in progress. More functionality will be added as it becomes more apparent what
 *       functionality is needed for implementing various layers and their features.
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
	 * @return Pointer to the packet
	 */
	Packet* NextPacket();

	/**
	 * @brief Get the number of remaining unassigned packets
	 *
	 * @return uint64_t
	 */
	uint64_t PktsRemaining();

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
	bool _first = true;
	std::list<Packet>::iterator _it;
	double _fwdPktChance;
};

} // namespace generator
