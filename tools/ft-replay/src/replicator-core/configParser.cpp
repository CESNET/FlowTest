/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator config parser implmentation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "configParser.hpp"

#include <stdexcept>

namespace replay {

void ConfigParser::AddUnitStrategyDescription(const StrategyDescription& unitStrategyDescription)
{
	_unitsStrategyDescription.emplace_back(unitStrategyDescription);
}

void ConfigParser::SetLoopStrategyDescription(const StrategyDescription& loopStrategyDescription)
{
	_loopStrategyDescription = loopStrategyDescription;
}

void ConfigParser::Validate()
{
	ValidateUnitsStrategyDescription();
	ValidateLoopStrategyDescription();
}

void ConfigParser::ValidateUnitsStrategyDescription()
{
	auto unitIpValidationRegex = CreateUnitIpValidationRegex();
	auto unitMacValidationRegex = CreateUnitMacValidationRegex();

	for (auto& unitStrategyDescription : _unitsStrategyDescription) {
		for (auto& [key, value] : unitStrategyDescription) {
			if (IsIpKey(key)) {
				MatchStrategyRegex(value, unitIpValidationRegex);
			} else if (IsMacKey(key)) {
				MatchStrategyRegex(value, unitMacValidationRegex);
			} else {
				_logger->error("Invalid Key name: " + key);
				throw std::runtime_error(
					"ConfigParser::ValidateUnitsStrategyDescription() has "
					"failed");
			}
		}
	}
}

void ConfigParser::ValidateLoopStrategyDescription()
{
	auto loopIpValidationRegex = CreateLoopIpValidationRegex();

	for (auto& [key, value] : _loopStrategyDescription) {
		if (IsIpKey(key)) {
			MatchStrategyRegex(value, loopIpValidationRegex);
		} else {
			_logger->error("Invalid Key name: " + key);
			throw std::runtime_error("ConfigParser::ValidateLoopStrategyDescription() has failed");
		}
	}
}

bool ConfigParser::IsIpKey(const std::string& key)
{
	return key == "srcip" || key == "dstip";
}

bool ConfigParser::IsMacKey(const std::string& key)
{
	return key == "srcmac" || key == "dstmac";
}

std::vector<std::regex> ConfigParser::CreateLoopIpValidationRegex() const
{
	return {std::regex("^None$"), std::regex("^addOffset\\(\\d+\\)$")};
}

std::vector<std::regex> ConfigParser::CreateUnitIpValidationRegex() const
{
	return {
		std::regex("^None$"),
		std::regex("^addConstant\\(\\d+\\)$"),
		std::regex("^addCounter\\(\\d+\\,\\s*\\d+\\)$")};
}

std::vector<std::regex> ConfigParser::CreateUnitMacValidationRegex() const
{
	return {std::regex("^None$"), std::regex("^([0-9A-Fa-f]{2}[:]){5}([0-9A-Fa-f]{2})$")};
}

void ConfigParser::MatchStrategyRegex(
	const std::string& strategy,
	const std::vector<std::regex>& regexes)
{
	bool match = false;
	for (auto& regex : regexes) {
		if (std::regex_match(strategy, regex)) {
			match = true;
			break;
		}
	}
	if (!match) {
		_logger->error("Invalid Strategy description: " + strategy);
		throw std::runtime_error("ConfigParser::MatchStrategyRegex() has failed");
	}
}

const ConfigParser::StrategyDescription& ConfigParser::GetLoopStrategyDescription() const
{
	return _loopStrategyDescription;
}

const std::vector<ConfigParser::StrategyDescription>&
ConfigParser::GetUnitsStrategyDescription() const
{
	return _unitsStrategyDescription;
}

} // namespace replay
