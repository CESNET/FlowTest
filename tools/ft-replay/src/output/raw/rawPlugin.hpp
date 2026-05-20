/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "rawQueue.hpp"

#include "logger.h"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace replay {

class RawPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct RawPlugin
	 *
	 * Constructs RawPlugin and RawQueue.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit RawPlugin(const std::string& params);

	/**
	 * @brief Default destructor
	 */
	~RawPlugin() = default;

	RawPlugin(const RawPlugin&) = delete;
	RawPlugin(RawPlugin&&) = delete;
	RawPlugin& operator=(const RawPlugin&) = delete;
	RawPlugin& operator=(RawPlugin&&) = delete;

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get MTU of the raw interface
	 */
	size_t GetMTU() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueId  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

	/**
	 * @brief Get NUMA node to which the NIC is connected
	 *
	 * @return ID of NUMA or nullopt
	 */
	NumaNode GetNumaNode() override;

private:
	/**
	 * @brief Parse arguments in map
	 *
	 * @param[in] argMap map with arguments
	 */
	void ParseMap(const std::map<std::string, std::string>& argMap);

	/**
	 * @brief Parse arguments
	 *
	 * Example format : arg1=value1,arg2=value2,...
	 *
	 * @param[in] args string with arguments, separated by comma
	 *
	 * @return pointer to OutputQueue
	 */
	int ParseArguments(const std::string& args);

	/**
	 * @brief Parses interfaces
	 *
	 * @param[in] value string with interfaces, two interfaces in case of multiport replay
	 */
	void ParseInterfaces(const std::string& value);

	void DeterminePacketSize();

	std::unique_ptr<RawQueue> _queue;
	std::vector<std::string> _ifcNames;
	size_t _burstSize = 1024;
	size_t _packetSize = 0;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawPlugin");
};

} // namespace replay
