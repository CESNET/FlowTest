/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parser factory implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "configParserFactory.hpp"

#include <stdexcept>

namespace replay {

std::unique_ptr<ConfigParser> ConfigParserFactory::Create(const std::string& configFilename)
{
	std::string fileExtension = ExtractFileExtension(configFilename);

	auto it = _RegisteredParsers.find(fileExtension);
	if (it == _RegisteredParsers.end()) {
		_logger->error("Config parser file extension: '" + fileExtension + "' is not registered.");
		throw std::runtime_error("ConfigParserFactory::Create() has failed");
	}

	return it->second(configFilename);
}

std::string ConfigParserFactory::ExtractFileExtension(const std::string& configFilename)
{
	std::size_t position = configFilename.find_last_of(".");
	if (position != std::string::npos) {
		return std::string(configFilename, position + 1);
	}

	_logger->error("Invalid config filename format. File extension is missing.");
	throw std::runtime_error("ConfigParserFactory::ExtractFileExtension() has failed");
}

bool ConfigParserFactory::RegisterParser(
	const std::string& fileExtension,
	const ConfigParserGenerator& funcCreate)
{
	return _RegisteredParsers.insert(std::make_pair(fileExtension, funcCreate)).second;
}

ConfigParserFactory& ConfigParserFactory::Instance()
{
	static ConfigParserFactory instance;
	return instance;
}

} // namespace replay
