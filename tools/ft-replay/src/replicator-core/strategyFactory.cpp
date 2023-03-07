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
StrategyFactory::CreateConcreteUnitStrategy(const std::string& strategy)
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
	throw std::runtime_error("Invalid strategy description: '" + strategy + "'");
}

template <>
ModifierStrategies::UnitMacStrategy
StrategyFactory::CreateConcreteUnitStrategy(const std::string& strategy)
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
	throw std::runtime_error("Invalid strategy description: '" + strategy + "'");
}

ModifierStrategies::LoopIpStrategy
StrategyFactory::CreateConcreteLoopStrategy(const std::string& strategy)
{
	std::smatch match;

	if (std::regex_search(strategy, match, std::regex("^addOffset\\((\\d+)\\)$"))) {
		return std::make_unique<LoopIpAddOffset>(std::stoul(match[1]));
	}
	if (std::regex_search(strategy, match, std::regex("^None$"))) {
		return std::make_unique<LoopNoneDefault>();
	}
	throw std::runtime_error("Invalid strategy description: '" + strategy + "'");
}

void StrategyFactory::CreateLoopStrategy(
	const ConfigParser::StrategyDescription& loopStrategyDescription)
{
	for (const auto& [key, strategy] : loopStrategyDescription) {
		CreateLoopStrategyByKey(key, strategy);
	}
}

void StrategyFactory::CreateLoopStrategyByKey(const std::string& key, const std::string& strategy)
{
	if (key == "srcip") {
		_replicatorStrategies.loopSrcIp = CreateConcreteLoopStrategy(strategy);
	} else if (key == "dstip") {
		_replicatorStrategies.loopDstIp = CreateConcreteLoopStrategy(strategy);
	} else {
		throw std::runtime_error("Invalid loop key name: '" + key + "'");
	}
}

void StrategyFactory::CreateUnitStrategy(
	const ConfigParser::StrategyDescription& unitStrategyDesciption)
{
	for (const auto& [key, strategy] : unitStrategyDesciption) {
		CreateUnitStrategyByKey(key, strategy);
	}
}

void StrategyFactory::CreateUnitStrategyByKey(const std::string& key, const std::string& strategy)
{
	if (key == "srcip") {
		_replicatorStrategies.unitSrcIp = CreateConcreteUnitStrategy<IpAddressView>(strategy);
	} else if (key == "dstip") {
		_replicatorStrategies.unitDstIp = CreateConcreteUnitStrategy<IpAddressView>(strategy);
	} else if (key == "srcmac") {
		_replicatorStrategies.unitSrcMac = CreateConcreteUnitStrategy<MacAddress>(strategy);
	} else if (key == "dstmac") {
		_replicatorStrategies.unitDstMac = CreateConcreteUnitStrategy<MacAddress>(strategy);
	} else {
		throw std::runtime_error("Invalid units key name: '" + key + "'");
	}
}

} // namespace replay
