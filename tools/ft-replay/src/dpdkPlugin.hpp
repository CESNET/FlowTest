/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief Dpdk plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "dpdkQueue.hpp"

#include "CStringArray/cStringArray.hpp"
#include "logger.h"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace replay {

/**
 * @brief DPDK output plugin
 *
 * DpdkPlugin provides the main API for the dpdk plugin.
 * It parses command line arguments, initializes dpdk,
 * dpdk ports and dpdkQueues. Based on the command line
 * arguments it is able to operate in two-port (multi-port)
 * replaying mode
 */
class DpdkPlugin : public OutputPlugin {
public:
	/**
	 * Constructor
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit DpdkPlugin(const std::string& params);

	/**
	 * Destructor
	 */
	~DpdkPlugin();

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get MTU of the used interface
	 */
	size_t GetMTU() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

private:
	int ParseArguments(const std::string& args);
	void ParseAddr(const std::string& value);
	void ParseMap(const std::map<std::string, std::string>& argMap);
	void FillDpdkArgs(CStringArray& array);
	int ConfigureDpdkPort(const std::string& PCIAddress);

	std::vector<std::unique_ptr<DpdkQueue>> _queues;
	std::vector<std::string> _PCIAddresses;
	size_t _memPoolSize = 8192;
	size_t _queueCount = 1;
	size_t _MTUSize = 1518;
	size_t _queueSize = 4096;
	size_t _burstSize = 64;

	std::vector<uint16_t> _ports;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("DpdkPlugin");
};

} // namespace replay
