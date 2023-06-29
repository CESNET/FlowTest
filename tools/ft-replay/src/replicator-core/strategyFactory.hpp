/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator strategy factory interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "configParser.hpp"
#include "logger.h"
#include "strategy.hpp"

#include <memory>
#include <string>
#include <vector>

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
	void CreateLoopScalarStrategyByKey(
		const ConfigParser::Key& key,
		const ConfigParser::Scalar& strategy);
	void CreateUnitStrategyByKey(const ConfigParser::Key& key, const ConfigParser::Value& strategy);
	void CreateUnitScalarStrategyByKey(
		const ConfigParser::Key& key,
		const ConfigParser::Scalar& strategy);
	void CreateUnitSequenceStrategyByKey(
		const ConfigParser::Key& key,
		const ConfigParser::Sequence& strategy);

	template <typename T>
	std::unique_ptr<UnitStrategy<T>>
	CreateConcreteUnitStrategy(const ConfigParser::Scalar& strategy);
	std::unique_ptr<LoopStrategy> CreateConcreteLoopStrategy(const ConfigParser::Scalar& strategy);
	std::vector<uint64_t> CreateUnitLoopOnlyStrategy(const ConfigParser::Sequence& strategy);
	std::vector<uint64_t> CreateUnitLoopOnlyStrategy(const ConfigParser::Scalar& strategy);
	ModifierStrategies _replicatorStrategies;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("StrategyFactory");
};

} // namespace replay
