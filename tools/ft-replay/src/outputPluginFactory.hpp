/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin factory interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputPlugin.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace replay {

/**
 * @brief Pointer to function which generate new Output plugin instance
 */
using OutputPluginGenerator = std::unique_ptr<OutputPlugin> (*)(const std::string&);

/**
 * @brief Output plugin factory pattern.
 *
 */
class OutputPluginFactory {
public:
	/**
	 * @brief Get factory instance - Singleton pattern
	 *
	 * @return OutputPluginFactory&  Reference to the factory singleton
	 */
	static OutputPluginFactory& Instance();

	/**
	 * @brief Create new output plugin
	 *
	 * @p OutputPluginParams format:
	 * "pluginName:pluginParam=value,nextPluginParam=value"
	 *
	 * pluginName = Registered plugin name
	 *
	 * @param OutputPluginParams Output plugin name and parameters string
	 * @return OutputPlugin* Pointer to output plugin context.
	 */
	std::unique_ptr<OutputPlugin> Create(const std::string& OutputPluginParams);

	/**
	 * @brief Register Output plugin
	 *
	 * @param pluginName Unique output plugin name
	 * @param funcCreate Output plugin instance generator
	 * @return true - if successfully registered, false otherwise
	 */
	bool RegisterPlugin(const std::string& pluginName, const OutputPluginGenerator& funcCreate);

private:
	std::pair<std::string, std::string>
	SplitOutputPluginParams(const std::string& OutputPluginParams);

	std::map<const std::string, OutputPluginGenerator> _registeredPlugins;
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("OutputPluginFactory");
};

} // namespace replay
