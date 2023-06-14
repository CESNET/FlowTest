/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator strategy factory implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "strategyFactory.hpp"

#include "ipAddress.hpp"
#include "macAddress.hpp"

#include <algorithm>
#include <memory>

namespace replay {

ModifierStrategies StrategyFactory::Create(
	const ConfigParser::StrategyDescription& unitStrategyDescription,
	const ConfigParser::StrategyDescription& loopStrategyDescription)
{
	_replicatorStrategies = {};
	CreateUnitStrategy(unitStrategyDescription);
	CreateLoopStrategy(loopStrategyDescription);
	return std::move(_replicatorStrategies);
}

template <>
ModifierStrategies::UnitIpStrategy
StrategyFactory::CreateConcreteUnitStrategy(const ConfigParser::Scalar& strategy)
{
	std::smatch match;

	if (std::regex_search(strategy, match, std::regex("^addConstant\\((\\d+)\\)$"))) {
		return std::make_unique<UnitIpAddConstant>(std::stoul(match[1]));
	}
	if (std::regex_search(strategy, match, std::regex("^addCounter\\((\\d+)\\,\\s*(\\d+)\\)$"))) {
		return std::make_unique<UnitIpAddCounter>(std::stoul(match[1]), std::stoul(match[2]));
	}
	if (std::regex_search(strategy, match, std::regex("^None$"))) {
		return std::make_unique<UnitNoneDefault<IpAddressView>>();
	}
	_logger->error("Invalid strategy description: '" + strategy + "'");
	throw std::runtime_error("StrategyFactory::CreateConcreteUnitStrategy() has failed");
}

template <>
ModifierStrategies::UnitMacStrategy
StrategyFactory::CreateConcreteUnitStrategy(const ConfigParser::Scalar& strategy)
{
	std::smatch match;

	if (std::regex_search(
			strategy,
			match,
			std::regex("^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$"))) {
		return std::make_unique<UnitMacSetAddress>(std::string(match[0]));
	}
	if (std::regex_search(strategy, match, std::regex("^None$"))) {
		return std::make_unique<UnitNoneDefault<MacAddress>>();
	}
	_logger->error("Invalid strategy description: '" + strategy + "'");
	throw std::runtime_error("StrategyFactory::CreateConcreteUnitStrategy() has failed");
}

ModifierStrategies::LoopIpStrategy
StrategyFactory::CreateConcreteLoopStrategy(const ConfigParser::Scalar& strategy)
{
	std::smatch match;

	if (std::regex_search(strategy, match, std::regex("^addOffset\\((\\d+)\\)$"))) {
		return std::make_unique<LoopIpAddOffset>(std::stoul(match[1]));
	}
	if (std::regex_search(strategy, match, std::regex("^None$"))) {
		return std::make_unique<LoopNoneDefault>();
	}
	_logger->error("Invalid strategy description: '" + strategy + "'");
	throw std::runtime_error("StrategyFactory::CreateConcreteLoopStrategy() has failed");
}

void StrategyFactory::CreateLoopStrategy(
	const ConfigParser::StrategyDescription& loopStrategyDescription)
{
	for (const auto& [key, strategy] : loopStrategyDescription) {
		CreateLoopScalarStrategyByKey(key, std::get<ConfigParser::Scalar>(strategy));
	}
}

void StrategyFactory::CreateLoopScalarStrategyByKey(
	const ConfigParser::Key& key,
	const ConfigParser::Scalar& strategy)
{
	if (key == "srcip") {
		_replicatorStrategies.loopSrcIp = CreateConcreteLoopStrategy(strategy);
	} else if (key == "dstip") {
		_replicatorStrategies.loopDstIp = CreateConcreteLoopStrategy(strategy);
	} else {
		_logger->error("Invalid loop key name: '" + key + "'");
		throw std::runtime_error("StrategyFactory::CreateLoopScalarStrategyByKey() has failed");
	}
}

void StrategyFactory::CreateUnitStrategy(
	const ConfigParser::StrategyDescription& unitStrategyDesciption)
{
	for (const auto& [key, strategy] : unitStrategyDesciption) {
		CreateUnitStrategyByKey(key, strategy);
	}
}

void StrategyFactory::CreateUnitStrategyByKey(
	const ConfigParser::Key& key,
	const ConfigParser::Value& strategy)
{
	if (std::holds_alternative<ConfigParser::Scalar>(strategy)) {
		CreateUnitScalarStrategyByKey(key, std::get<ConfigParser::Scalar>(strategy));
	} else if (std::holds_alternative<ConfigParser::Sequence>(strategy)) {
		CreateUnitSequenceStrategyByKey(key, std::get<ConfigParser::Sequence>(strategy));
	}
}

void StrategyFactory::CreateUnitScalarStrategyByKey(
	const ConfigParser::Key& key,
	const ConfigParser::Scalar& strategy)
{
	if (key == "srcip") {
		_replicatorStrategies.unitSrcIp = CreateConcreteUnitStrategy<IpAddressView>(strategy);
	} else if (key == "dstip") {
		_replicatorStrategies.unitDstIp = CreateConcreteUnitStrategy<IpAddressView>(strategy);
	} else if (key == "srcmac") {
		_replicatorStrategies.unitSrcMac = CreateConcreteUnitStrategy<MacAddress>(strategy);
	} else if (key == "dstmac") {
		_replicatorStrategies.unitDstMac = CreateConcreteUnitStrategy<MacAddress>(strategy);
	} else if (key == "loopOnly") {
		_replicatorStrategies.loopOnly = CreateUnitLoopOnlyStrategy(strategy);
	} else {
		_logger->error("Invalid units key name: '" + key + "'");
		throw std::runtime_error("StrategyFactory::CreateUnitScalarStrategyByKey() has failed");
	}
}

void StrategyFactory::CreateUnitSequenceStrategyByKey(
	const ConfigParser::Key& key,
	const ConfigParser::Sequence& strategy)
{
	if (key == "loopOnly") {
		if (strategy.empty()) {
			// if sequence is empty add value uint64_t::max() as "disable" value
			std::string maxValueAsDisabled = std::to_string(std::numeric_limits<uint64_t>::max());
			_replicatorStrategies.loopOnly = CreateUnitLoopOnlyStrategy({maxValueAsDisabled});
		} else {
			_replicatorStrategies.loopOnly = CreateUnitLoopOnlyStrategy(strategy);
		}
	} else {
		_logger->error("Invalid units key name: '" + key + "'");
		throw std::runtime_error("StrategyFactory::CreateUnitSequenceStrategyByKey() has failed");
	}
}

std::vector<uint64_t>
StrategyFactory::CreateUnitLoopOnlyStrategy(const ConfigParser::Scalar& strategy)
{
	if (strategy == "All") {
		return {};
	} else {
		return CreateUnitLoopOnlyStrategy(ConfigParser::Sequence {strategy});
	}
}

std::vector<uint64_t>
StrategyFactory::CreateUnitLoopOnlyStrategy(const ConfigParser::Sequence& strategy)
{
	std::vector<uint64_t> loopOnly;
	for (const auto& value : strategy) {
		size_t dashPosition = value.find('-');
		if (dashPosition != std::string::npos) {
			std::string startStr = value.substr(0, dashPosition);
			std::string endStr = value.substr(dashPosition + 1);

			uint64_t start = std::stoull(startStr);
			uint64_t end = std::stoull(endStr);

			if (start > end) {
				_logger->error("Invalid LoopOnly value (rangeStart > rangeEnd)");
				throw std::runtime_error(
					"StrategyFactory::CreateUnitLoopOnlyStrategy() has failed");
			}

			for (uint64_t i = start; i <= end; ++i) {
				loopOnly.emplace_back(i);
			}
		} else {
			uint64_t num = std::stoull(value);
			loopOnly.emplace_back(num);
		}
	}

	std::sort(loopOnly.begin(), loopOnly.end());
	loopOnly.erase(std::unique(loopOnly.begin(), loopOnly.end()), loopOnly.end());

	return loopOnly;
}

} // namespace replay
