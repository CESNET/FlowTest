/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator strategy factory interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "configParser.hpp"
#include "strategy.hpp"

#include <memory>
#include <string>

#pragma once

namespace replay {

/**
 * @brief Construct a strategy from given description
 */
class StrategyFactory {
public:
	/**
	 * @brief Create a ModifierStrategies structure from given descriptions
	 *
	 * @param unitStrategyDescription Description of replication units
	 * @param loopStrategyDescription Description of loop strategy
	 * @return ModifierStrategies Pakcet modifier strategies
	 */
	ModifierStrategies Create(
		const ConfigParser::StrategyDescription& unitStrategyDescription,
		const ConfigParser::StrategyDescription& loopStrategyDescription);

private:
	void CreateLoopStrategy(const ConfigParser::StrategyDescription& loopStrategyDesciption);
	void CreateUnitStrategy(const ConfigParser::StrategyDescription& unitStrategyDesciption);
	void CreateLoopStrategyByKey(const std::string& key, const std::string& strategy);
	void CreateUnitStrategyByKey(const std::string& key, const std::string& strategy);

	template <typename T>
	std::unique_ptr<UnitStrategy<T>> CreateConcreteUnitStrategy(const std::string& strategy);
	std::unique_ptr<LoopStrategy> CreateConcreteLoopStrategy(const std::string& strategy);
	ModifierStrategies _replicatorStrategies;
};

} // namespace replay
