/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief Thread manager interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace replay {

/**
 * @typedef NumaNode
 * @brief Type alias for NUMA node ID
 *
 * Contains either the NUMA node ID or std::nullopt
 * if the ID is not specified
 */
using NumaNode = std::optional<size_t>;

/**
 * @brief Sets the initial affinity
 *
 * @param[in] cores Set of desired cores
 *
 * Sets the affinity to the specified cores
 */
void SetInitialAffinity(const std::set<size_t>& cores);

/**
 * @class ThreadManager
 * @brief Manages thread affinity
 *
 * ThreadManager has multiple modes of operation
 * and selects one based on the informations recieved
 * in its constructor.
 *
 * The decisive informations are:
 * 	NUMA node, to which the NIC used for replaying is connected.
 * 	Desired cores, to which the user wants to pin the working threads
 *
 * If the desired cores are specified, then the manager
 * pins the active thread to the desired cores ( Only to one core at a time,
 * based on the queueId passes to SetThreadAffinity ). If NUMA information
 * is given as well then the manager provides more detailed warnings if the
 * desired cores specified are not optimal.
 *
 * If the desired cores are not specified, then the manager
 * sets the affinity mask either to all cores or all cores on a specified
 * NUMA node based on whether or not the NUMA information is given.
 *
 */

class ThreadManager {
public:
	/**
	 * @brief Constructor
	 *
	 * @param[in] cpuSet Input arguments in 'core,core,...' format
	 * @param[in] numaNode ID of NUMA node, std::nullopt if not specified
	 * @param[in] queueCount Number of queues used by the output plugin
	 */
	ThreadManager(const std::set<size_t>& cpuSet, NumaNode numaNode, size_t queueCount);

	/**
	 * @brief Sets the thread affinity of the given output queue
	 *
	 * @param[in] queueId ID of a queue to bind to cores
	 *
	 * The affinity to set is determined by
	 * the desired cores and NUMA node information
	 * recieved in the class constructor.
	 */
	void SetThreadAffinity(size_t queueId);

	/**
	 * @brief Resets the thread affinity
	 *
	 * Sets the thread affinity to all cores
	 * on the NUMA node recieved from the class constructor.
	 * If the ThreadManager did not recieve any NUMA information
	 * the affinity is set to all cores availible on the system.
	 */
	void ResetThreadAffinity();

private:
	std::vector<size_t> GetAllCoresInfo();
	std::vector<size_t> GetNumaInfo(size_t nodeId);
	void VerifyCores();

	std::vector<size_t> _numaCores;
	std::vector<size_t> _specifiedCores;
	NumaNode _numaNode;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("ThreadManager");
};

} // namespace replay
