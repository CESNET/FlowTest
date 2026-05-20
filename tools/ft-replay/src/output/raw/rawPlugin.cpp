/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rawPlugin.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "protocol/ethernet.hpp"
#include "utils.hpp"

#include <cerrno>
#include <map>
#include <memory>

namespace replay {

OutputPluginFactoryRegistrator<RawPlugin> rawPluginRegistration("raw");

RawPlugin::RawPlugin(const std::string& params)
{
	ParseArguments(params);
	DeterminePacketSize();
	_queue = std::make_unique<RawQueue>(_ifcNames, _packetSize, _burstSize);
}

void RawPlugin::DeterminePacketSize()
{
	// Compute the minimal supported MTU from the interfaces
	size_t maxSize = std::numeric_limits<size_t>::max();

	for (const std::string& ifcName : _ifcNames) {
		size_t currSize = utils::GetInterfaceMTU(ifcName);
		if (currSize < maxSize) {
			maxSize = currSize;
		}
	}

	maxSize += protocol::Ethernet::HEADER_SIZE;

	if (_packetSize && _packetSize > maxSize) {
		_logger->error(
			"Packet size ({} bytes) is bigger than interface MTU (+ Ethernet header size) ({} "
			"bytes)",
			_packetSize,
			maxSize);
		throw std::invalid_argument("RawPlugin::DeterminePacketSize() has failed");
	}
	if (!_packetSize) {
		_packetSize = maxSize;
		_logger->info(
			"Packet size not specified, using interface MTU (+ Ethernet header size) ({} bytes)",
			_packetSize);
	}
}

size_t RawPlugin::GetQueueCount() const noexcept
{
	return 1;
}

size_t RawPlugin::GetMTU() const noexcept
{
	return _packetSize;
}

OutputQueue* RawPlugin::GetQueue(uint16_t queueId)
{
	if (queueId != 0 || _queue == nullptr) {
		_logger->error("Invalid request for OutputQueue");
		throw std::runtime_error("RawPlugin::GetQueue() has failed");
	}
	return _queue.get();
}

NumaNode RawPlugin::GetNumaNode()
{
	NumaNode result = std::nullopt;

	for (std::string& ifcName : _ifcNames) {
		NumaNode ret = utils::GetInterfaceNumaNode(ifcName);
		if (!ret.has_value()) {
			// Unable to determine the NUMA node
			continue;
		}

		if (!result.has_value()) {
			result = ret;
			continue;
		}

		if (result.value() != ret.value()) {
			_logger->warn("Specified Interfaces are connected to different NUMA nodes");
			return std::nullopt;
		}
	}

	return result;
}

void RawPlugin::ParseInterfaces(const std::string& value)
{
	auto ret = value.find('#');
	if (ret == std::string::npos) {
		_ifcNames.push_back(value);
	} else {
		_ifcNames.push_back(value.substr(0, ret));
		_ifcNames.push_back(value.substr(ret + 1));
	}
}

void RawPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "ifc") {
			ParseInterfaces(value);
		} else if (key == "packetSize") {
			_packetSize = std::stoul(value);
		} else if (key == "burstSize") {
			_burstSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("RawPlugin::ParseMap() has failed");
		}
	}
}

int RawPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParseMap(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error("Parameter \"packetSize\" or \"burstSize\" wrong format");
		throw std::invalid_argument("RawPlugin::ParseArguments() has failed");
	}

	if (_ifcNames.empty()) {
		_logger->error("Required parameter \"ifc\" missing/empty");
		throw std::invalid_argument("RawPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
