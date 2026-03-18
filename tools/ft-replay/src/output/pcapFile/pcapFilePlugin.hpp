/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"
#include "pcapFileQueue.hpp"

namespace replay {

class PcapFilePlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct PcapFilePlugin
	 *
	 * Constructs PcapFilePlugin and PcapFileQueues.
	 *
	 * @param[in] params string "arg1=value1,arg2=value2"
	 */
	explicit PcapFilePlugin(const std::string& params);

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get MTU of the pcapFile interface
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

private:
	int ParseArguments(const std::string& args);
	void ParseMap(const std::map<std::string, std::string>& argMap);
	std::string AppendQueueIdToFileName(uint16_t queueId);

	std::vector<std::unique_ptr<PcapFileQueue>> _queues;
	std::string _fileName;
	bool _multiPort = false;
	size_t _burstSize = 1024;
	size_t _packetSize = 2048;
	size_t _queueCount = 1;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("PcapFilePlugin");
};

} // namespace replay
