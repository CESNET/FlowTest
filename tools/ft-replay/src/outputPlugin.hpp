/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config.hpp"
#include "logger.h"
#include "offloads.hpp"
#include "threadManager.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace replay {

class OutputQueue;

/**
 * @brief Output plugin interface
 *
 */
class OutputPlugin {
public:
	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	virtual size_t GetQueueCount() const noexcept = 0;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	virtual OutputQueue* GetQueue(uint16_t queueId) = 0;

	/**
	 * @brief Default virtual destructor
	 */
	virtual ~OutputPlugin() = default;

	/**
	 * @brief Determines and configure the available offloads.
	 *
	 * @param offloads The requested offloads to configure.
	 * @return Configured offloads.
	 */
	virtual Offloads ConfigureOffloads(const OffloadRequests& offloads);

	/**
	 * @brief Get NUMA node to which the NIC is connected
	 *
	 * @return ID of NUMA or nullopt
	 */
	virtual NumaNode GetNumaNode();

	/**
	 * @brief Get MTU of the output interface
	 */
	virtual size_t GetMTU() const noexcept = 0;

protected:
	/**
	 * @brief Split arguments into map
	 *
	 * @param[in] string with arguments, separated by comma
	 *
	 * @return map<argName, argValue>
	 */
	std::map<std::string, std::string> SplitArguments(const std::string& args) const;
};

} // namespace replay
