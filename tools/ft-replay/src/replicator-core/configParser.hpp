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
#include <variant>
#include <vector>

namespace replay {

/**
 * @brief Interface of Replicator config file parser
 */
class ConfigParser {
public:
	using Key = std::string;
	using Scalar = std::string;
	using Sequence = std::vector<std::string>;
	using Value = std::variant<Scalar, Sequence>;
	using KeyValue = std::pair<Key, Value>;
	using Dictionary = std::map<Key, Value>;
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

	bool IsIpKey(const Key& key);
	bool IsMacKey(const Key& key);
	bool IsLoopOnlyKey(const Key& key);

	void AssertScalarType(const Value& value) const;

	std::vector<std::regex> CreateLoopIpValidationRegex() const;
	std::vector<std::regex> CreateUnitIpValidationRegex() const;
	std::vector<std::regex> CreateUnitMacValidationRegex() const;
	std::vector<std::regex> CreateUnitLoopOnlyValidationRegex() const;

	void MatchRegex(const Value& valueToMatch, const std::vector<std::regex>& regexes);
	void MatchScalarRegex(const Scalar& valueToMatch, const std::vector<std::regex>& regexes);

	std::vector<StrategyDescription> _unitsStrategyDescription;
	StrategyDescription _loopStrategyDescription;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("ConfigParser");
};

} // namespace replay
