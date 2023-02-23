/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Factory config parser file extension registrator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "configParserFactory.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace replay {

/**
 * @brief Config parser registrator.
 *
 * @tparam T Type of class to register
 */
template <typename T>
struct ConfigParserFactoryRegistrator {
	/**
	 * @brief Register config parser to the factory.
	 *
	 * @param fileExtension  Config file extension name
	 */
	ConfigParserFactoryRegistrator(const std::string& fileExtension)
	{
		bool inserted;
		inserted = ConfigParserFactory::Instance().RegisterParser(
			fileExtension,
			[](const std::string& configFilename) -> std::unique_ptr<ConfigParser> {
				return std::make_unique<T>(configFilename);
			});

		if (!inserted) {
			throw std::runtime_error(
				"Multiple registration of Config parser file extensions: " + fileExtension);
		}
	}
};

} // namespace replay
