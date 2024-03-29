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
	auto unitLoopOnlyValidationRegex = CreateUnitLoopOnlyValidationRegex();

	for (auto& unitStrategyDescription : _unitsStrategyDescription) {
		for (auto& [key, value] : unitStrategyDescription) {
			if (IsIpKey(key)) {
				AssertScalarType(value);
				MatchRegex(value, unitIpValidationRegex);
			} else if (IsMacKey(key)) {
				AssertScalarType(value);
				MatchRegex(value, unitMacValidationRegex);
			} else if (IsLoopOnlyKey(key)) {
				MatchRegex(value, unitLoopOnlyValidationRegex);
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
			AssertScalarType(value);
			MatchRegex(value, loopIpValidationRegex);
		} else {
			_logger->error("Invalid Key name: " + key);
			throw std::runtime_error("ConfigParser::ValidateLoopStrategyDescription() has failed");
		}
	}
}

bool ConfigParser::IsIpKey(const Key& key)
{
	return key == "srcip" || key == "dstip";
}

bool ConfigParser::IsMacKey(const Key& key)
{
	return key == "srcmac" || key == "dstmac";
}

bool ConfigParser::IsLoopOnlyKey(const Key& key)
{
	return key == "loopOnly";
}

void ConfigParser::AssertScalarType(const Value& value) const
{
	if (!std::holds_alternative<Scalar>(value)) {
		_logger->error("Invalid entry type (scalar value expected)");
		throw std::runtime_error("ConfigParser::AssertScalarType() has failed");
	}
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

std::vector<std::regex> ConfigParser::CreateUnitLoopOnlyValidationRegex() const
{
	return {std::regex("^(\\d+(?:-\\d+)?)|(All)$")};
}

void ConfigParser::MatchRegex(const Value& valueToMatch, const std::vector<std::regex>& regexes)
{
	if (std::holds_alternative<Scalar>(valueToMatch)) {
		MatchScalarRegex(std::get<Scalar>(valueToMatch), regexes);
	} else if (std::holds_alternative<Sequence>(valueToMatch)) {
		for (const auto& scalarValue : std::get<Sequence>(valueToMatch)) {
			MatchScalarRegex(scalarValue, regexes);
		}
	} else {
		_logger->error("Invalid variant alternative");
		throw std::runtime_error("ConfigParser::MatchRegex() has failed");
	}
}

void ConfigParser::MatchScalarRegex(
	const Scalar& valueToMatch,
	const std::vector<std::regex>& regexes)
{
	bool match = false;
	for (auto& regex : regexes) {
		if (std::regex_match(valueToMatch, regex)) {
			match = true;
			break;
		}
	}
	if (!match) {
		_logger->error("Invalid description: '" + valueToMatch + "'");
		throw std::runtime_error("ConfigParser::MatchScalarRegex() has failed");
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
