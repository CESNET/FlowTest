/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator Yaml config file parser interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "configParser.hpp"
#include "configParserFactoryRegistrator.hpp"
#include "logger.h"

#include <string>

#include <yaml-cpp/yaml.h>

namespace replay {

/**
 * @brief Replicator config file parser in Yaml format
 */
class YamlConfigParser : public ConfigParser {
public:
	/**
	 * @brief Open and parse config file
	 *
	 * @param configFilename Path to configuration file
	 * @throw std::runtime_error TODO
	 */
	YamlConfigParser(const std::string& configFilename);

private:
	using YamlNode = YAML::Node;
	using YamlIterator = YAML::detail::iterator_value;
	using SectionParser = void (YamlConfigParser::*)(const YamlNode&);

	void Parse();
	YamlNode GetNodeByName(const std::string& nodeName);
	KeyValue ParseKeyValue(const YamlIterator& keyValueIterator);
	Dictionary ParseDictionary(const YamlNode& node, const std::string& nodeName);
	bool IsNodeExists(const std::string& nodeName);

	void SearchForUnknownSections();
	void LoadFileAsNode(const std::string& configFilename);
	void ParseSection(const std::string& sectionName, SectionParser sectionParser);
	void ParseUnitsAnonymousNodes(const YamlNode& anonymousUnitsNodes);
	void ParseUnitsNode(const YamlNode& unitNode);
	void ParseLoopNode(const YamlNode& loopNode);

	YamlNode _rootNode;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("YamlConfigParser");
};

} // namespace replay
