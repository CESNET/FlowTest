/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parser factory interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "configParser.hpp"
#include "logger.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace replay {

/**
 * @brief Pointer to function which generate new Config parser instance
 */
using ConfigParserGenerator = std::function<std::unique_ptr<ConfigParser>(const std::string&)>;

/**
 * @brief Config parser factory pattern.
 *
 */
class ConfigParserFactory {
public:
	/**
	 * @brief Get factory instance - Singleton pattern
	 *
	 * @return ConfigParserFactory&  Reference to the factory singleton
	 */
	static ConfigParserFactory& Instance();

	/**
	 * @brief Create new config parser
	 *
	 * @p configFilename format:
	 * "configFilename.extension"
	 *
	 * extension = Registered file extension name
	 *
	 * @param configFilename Name of configuration file
	 * @return ConfigParser* Pointer to config parser context.
	 */
	std::unique_ptr<ConfigParser> Create(const std::string& configFilename);

	/**
	 * @brief Register parser file extension
	 *
	 * @param fileExtension Unique file extension
	 * @param funcCreate Config parser instance generator
	 * @return true - if successfully registered, false otherwise
	 */
	bool RegisterParser(const std::string& fileExtension, const ConfigParserGenerator& funcCreate);

private:
	std::string ExtractFileExtension(const std::string& configFilename);
	std::map<const std::string, ConfigParserGenerator> _RegisteredParsers;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("ConfigParserFactory");
};

} // namespace replay
