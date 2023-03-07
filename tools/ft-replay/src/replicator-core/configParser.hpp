/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator config parser interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace replay {

/**
 * @brief Interface of Replicator config file parser
 */
class ConfigParser {
public:
	using KeyValue = std::pair<std::string, std::string>;
	using Dictionary = std::map<std::string, std::string>;
	using StrategyDescription = Dictionary;

	/**
	 * @brief Get the Loop Strategy Description
	 */
	const StrategyDescription& GetLoopStrategyDescription() const;

	/**
	 * @brief Get vector of Unit Strategy Description
	 */
	const std::vector<StrategyDescription>& GetUnitsStrategyDescription() const;

protected:
	/**
	 * @brief Validate loaded configuration
	 */
	void Validate();

	/**
	 * @brief Add description to the vector
	 */
	void AddUnitStrategyDescription(const StrategyDescription& unitStrategyDescription);
	/**
	 * @brief Set the Loop Strategy Description
	 */
	void SetLoopStrategyDescription(const StrategyDescription& loopStrategyDescription);

	/**
	 * @brief Loop section identifier name
	 */
	const std::string _loopIdentifier = "loop";
	/**
	 * @brief Units section identifier name
	 */
	const std::string _unitsIdentifier = "units";

private:
	void ValidateUnitsStrategyDescription();
	void ValidateLoopStrategyDescription();

	bool IsIpKey(const std::string& key);
	bool IsMacKey(const std::string& key);

	std::vector<std::regex> CreateLoopIpValidationRegex() const;
	std::vector<std::regex> CreateUnitIpValidationRegex() const;
	std::vector<std::regex> CreateUnitMacValidationRegex() const;

	void MatchStrategyRegex(const std::string& strategy, const std::vector<std::regex>& regexes);

	std::vector<StrategyDescription> _unitsStrategyDescription;
	StrategyDescription _loopStrategyDescription;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("ConfigParser");
};

} // namespace replay
