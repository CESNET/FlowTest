/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Pcap file plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcapFilePlugin.hpp"
#include "pcapFileQueue.hpp"
#include "utils.hpp"

#include "outputPluginFactoryRegistrator.hpp"

#include <cstddef>
#include <ctime>
#include <map>

namespace replay {

// register pcap file plugin to the factory
OutputPluginFactoryRegistrator<PcapFilePlugin> pcapFilePluginRegistration("pcapFile");

PcapFilePlugin::PcapFilePlugin(const std::string& params)
{
	ParseArguments(params);

	for (size_t queueId = 0; queueId < _queueCount; queueId++) {
		auto decoratedFileName = AppendQueueIdToFileName(queueId);
		_queues.emplace_back(
			std::make_unique<PcapFileQueue>(
				decoratedFileName,
				_packetSize,
				_burstSize,
				_multiPort));
	}
}

std::string PcapFilePlugin::AppendQueueIdToFileName(uint16_t queueId)
{
	// append queue id only in multi queue mode
	if (_queueCount == 1) {
		return _fileName;
	}

	return _fileName + "." + std::to_string(queueId);
}

size_t PcapFilePlugin::GetQueueCount() const noexcept
{
	return _queueCount;
}

OutputQueue* PcapFilePlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

size_t PcapFilePlugin::GetMTU() const noexcept
{
	return _packetSize;
}

void PcapFilePlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "file") {
			_fileName = value;
		} else if (key == "queueCount") {
			_queueCount = std::stoul(value);
		} else if (key == "packetSize") {
			_packetSize = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else if (key == "multiPort") {
			_multiPort = utils::StrToBool(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("PcapFilePlugin::ParseMap() has failed");
		}
	}
}

int PcapFilePlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error(
			"Parameter \"packetSize\", \"burstSize\" or \"queueCount\" has wrong format");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	if (_fileName.empty()) {
		_logger->error("Required parameter \"file\" missing/empty");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	if (!_queueCount) {
		_logger->error("Parameter \"queueCount\" has to be bigger than 0");
		throw std::invalid_argument("PcapFilePlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
