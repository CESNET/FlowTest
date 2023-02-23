/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator Yaml config file parser implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "yamlConfigParser.hpp"

#include <stdexcept>
#include <utility>

#include <iostream>

namespace replay {

static ConfigParserFactoryRegistrator<YamlConfigParser> yamlConfigParserExtension("yaml");

YamlConfigParser::YamlConfigParser(const std::string& configFilename)
{
	LoadFileAsNode(configFilename);
	Parse();
	Validate();
}

void YamlConfigParser::LoadFileAsNode(const std::string& configFilename)
{
	_rootNode = YAML::LoadFile(configFilename);
}

YamlConfigParser::YamlNode YamlConfigParser::GetNodeByName(const std::string& nodeName)
{
	return _rootNode[nodeName];
}

bool YamlConfigParser::IsNodeExists(const std::string& nodeName)
{
	return static_cast<bool>(_rootNode[nodeName]);
}

void YamlConfigParser::Parse()
{
	ParseSection(_unitsIdentifier, &YamlConfigParser::ParseUnitsAnonymousNodes);
	ParseSection(_loopIdentifier, &YamlConfigParser::ParseLoopNode);
	SearchForUnknownSections();
}

void YamlConfigParser::SearchForUnknownSections()
{
	static const std::vector<std::string> ValidSectionNames = {
		_unitsIdentifier,
		_loopIdentifier,
	};

	for (const auto& sectionIterator : _rootNode) {
		std::string sectionName = sectionIterator.first.as<std::string>();
		if (std::find(ValidSectionNames.begin(), ValidSectionNames.end(), sectionName)
			== ValidSectionNames.end()) {
			_logger->warn("Config section '" + sectionName + "' is unknown");
		}
	}
}

void YamlConfigParser::ParseSection(const std::string& sectionName, SectionParser sectionParser)
{
	if (IsNodeExists(sectionName)) {
		YamlNode node = GetNodeByName(sectionName);
		(this->*sectionParser)(node);
	} else {
		_logger->error("Config section '" + sectionName + "' is missing");
		throw std::runtime_error("YamlConfigParser::ParseSection() has failed");
	}
}

void YamlConfigParser::ParseUnitsAnonymousNodes(const YamlNode& anonymousUnitsNodes)
{
	for (auto unitsNodeIterator : anonymousUnitsNodes) {
		ParseUnitsNode(unitsNodeIterator);
	}
}

void YamlConfigParser::ParseUnitsNode(const YamlNode& unitNode)
{
	Dictionary unitStrategyDescription;
	unitStrategyDescription = ParseDictionary(unitNode, _unitsIdentifier);
	AddUnitStrategyDescription(unitStrategyDescription);
}

void YamlConfigParser::ParseLoopNode(const YamlNode& loopNode)
{
	Dictionary loopStrategyDescription;
	loopStrategyDescription = ParseDictionary(loopNode, _loopIdentifier);
	SetLoopStrategyDescription(loopStrategyDescription);
}

ConfigParser::Dictionary
YamlConfigParser::ParseDictionary(const YamlNode& node, const std::string& nodeName)
{
	Dictionary dictionary;
	for (auto keyValueIterator : node) {
		KeyValue keyValue = ParseKeyValue(keyValueIterator);
		auto [_, isInserted] = dictionary.emplace(keyValue);
		if (!isInserted) {
			_logger->error("Duplicated item: " + nodeName + "->" + keyValue.first);
			throw std::runtime_error("YamlConfigParser::ParseDictionary() has failed");
		}
	}
	return dictionary;
}

ConfigParser::KeyValue YamlConfigParser::ParseKeyValue(const YamlIterator& keyValueIterator)
{
	return std::make_pair(
		keyValueIterator.first.as<std::string>(),
		keyValueIterator.second.as<std::string>());
}

} // namespace replay
