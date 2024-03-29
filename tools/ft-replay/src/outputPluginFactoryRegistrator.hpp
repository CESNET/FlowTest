/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin registration to factory
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "outputPluginFactory.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace replay {

/**
 * @brief Output plugin registration.
 *
 * @tparam T  Type of class to register
 */
template <typename T>
struct OutputPluginFactoryRegistrator {
	/**
	 * @brief Register Output plugin to the factory.
	 *
	 * @param pluginName  Unique plugin name
	 */
	explicit OutputPluginFactoryRegistrator(const std::string& pluginName)
	{
		bool inserted;
		inserted = OutputPluginFactory::Instance().RegisterPlugin(
			pluginName,
			[](const std::string& params) -> std::unique_ptr<OutputPlugin> {
				return std::make_unique<T>(params);
			});
		if (!inserted) {
			throw std::runtime_error("Multiple registration of Output plugin: " + pluginName);
		}
	}
};

} // namespace replay
