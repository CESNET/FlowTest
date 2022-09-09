/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin factory implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "outputPluginFactory.hpp"

#include <iostream>

namespace replay {

std::unique_ptr<OutputPlugin> OutputPluginFactory::Create(const std::string& OutputPluginParams)
{
	auto [pluginName, pluginParams] = SplitOutputPluginParams(OutputPluginParams);

	auto it = _registeredPlugins.find(pluginName);
	if (it == _registeredPlugins.end()) {
		_logger->info("Output plugin: '" + pluginName + "' is not registered.");
		throw std::runtime_error("OutputPluginFactory::Create() has failed");
	}

	return it->second(pluginParams);
}

std::pair<std::string, std::string> OutputPluginFactory::SplitOutputPluginParams(
	const std::string& OutputPluginParams)
{
	std::size_t position = OutputPluginParams.find_first_of(":");
	if (position == std::string::npos) {
		_logger->info("Invalid format of Output plugin parameters");
		throw std::runtime_error("OutputPluginFactory::SplitOutputPluginParams() has failed");
	}

	return std::make_pair(
		std::string(OutputPluginParams, 0, position),
		std::string(OutputPluginParams, position + 1));
}

bool OutputPluginFactory::RegisterPlugin(
	const std::string& pluginName,
	const OutputPluginGenerator& funcCreate)
{
	return _registeredPlugins.insert(std::make_pair(pluginName, funcCreate)).second;
}

OutputPluginFactory& OutputPluginFactory::instance()
{
	static OutputPluginFactory instance;
	return instance;
}

} // namespace replay
