/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Module of common functions for parsing configurations
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief Split a string using a delimiter
 *
 * @param s                          The string to split
 * @param delimiter                  The delimiter to split on
 * @return std::vector<std::string>  The pieces
 */
std::vector<std::string> StringSplit(const std::string& s, const std::string& delimiter);

/**
 * @brief Parse a value, wraps around std::from_chars
 *
 * @tparam T  The value type to parse
 * @param s   The string to parse the value from
 * @return std::optional<T>  The parse value or std::nullopt
 */
template <typename T>
std::optional<T> ParseValue(const std::string& s);

template <>
std::optional<double> ParseValue(const std::string& s);

/**
 * @brief Remove whitespace from the beginning and end of a string
 *
 * @param s
 * @return std::string
 */
std::string StringStrip(std::string s);

/**
 * @brief Check if a string ends with a suffix and strip it
 *
 * @param s[inout]  The string to check and strip, will be modified in-place
 * @param suffix  The suffix
 *
 * @return true if the suffix was found and stripped, else false
 */
bool StringCheckStripSuffix(std::string& s, const std::string& suffix);

/**
 * @brief Expect the node to be a yaml sequence
 *
 * @param node
 *
 * @throw ConfigError in case it is not
 */
void ExpectSequence(const YAML::Node& node);

/**
 * @brief Expect the node to be a yaml map or throw an error
 *
 * @param node
 *
 * @throw ConfigError in case it is not
 */
void ExpectMap(const YAML::Node& node);

/**
 * @brief Expect the node to be a yaml scalar or throw an error
 *
 * @param node
 *
 * @throw ConfigError in case it is not
 */
void ExpectScalar(const YAML::Node& node);

/**
 * @brief Expect the node to contain a key or throw an error
 *
 * @param node
 *
 * @throw ConfigError in case it does not
 */
void ExpectKey(const YAML::Node& node, const std::string& key);

/**
 * @brief Expect the node to be a yaml scalar and return its string representation
 *
 * @param node
 *
 * @return The string representation of the yaml scalar
 *
 * @throw ConfigError in case it is not
 */
std::string AsScalar(const YAML::Node& node);

/**
 * @brief Check that the node only contains the keys in the allowed keys field
 *
 * @param node         The yaml node
 * @param allowedKeys  The keys the node can contain
 *
 * @throw ConfigError in case there are extra keys
 */
void CheckAllowedKeys(const YAML::Node& node, const std::vector<std::string>& allowedKeys);

/**
 * @brief Parse a probability as either a double or a percentage
 *
 * @param node YAML node containing the probability
 * @return The probability in the range <0.0, 1.0>
 *
 * @throw ConfigError on errornous value
 */
double ParseProbability(const YAML::Node& node);

/**
 * @brief Parse a time unit as ns/us/ms/s
 *
 * @param node YAML node containing the speed unit
 * @return The value represented in nanoseconds
 *
 * @throw ConfigError on errornous value
 */
uint64_t ParseTimeUnit(const YAML::Node& node);

/**
 * @brief Parse a speed unit as bits of kbps/mbps/gbps
 *
 * @param node YAML node containing the speed unit
 * @return The value represented in bits per second
 *
 * @throw ConfigError on errornous value
 */
uint64_t ParseSpeedUnit(const YAML::Node& node);

/**
 * @brief Parse a YAML sequence into a vector of configuration objects
 * The configuration object must be constructible from a yaml node
 *
 * @tparam T               The configuration object
 * @param node             The yaml node containing the sequence
 * @return std::vector<T>  Vector of parsed configuraiton objects
 */
template <typename T>
std::vector<T> ParseMany(const YAML::Node& node);

/**
 * @brief Same as parseMany but can also be a scalar instead of a sequence,
 * in which case a vector of one is returned
 *
 * @tparam T               The configuration object
 * @param node             The yaml node containing the sequence
 * @return std::vector<T>  Vector of parsed configuraiton objects
 */
template <typename T>
std::vector<T> ParseOneOrMany(const YAML::Node& node);

/**
 * @brief An exception representing an error in the yaml configuration
 *
 */
class ConfigError : public std::exception {
public:
	/**
	 * @brief Construct a new Config Error object
	 *
	 * @param node  The yaml node where the error happened
	 * @param error The error description
	 */
	ConfigError(const YAML::Node& node, const std::string& error)
		: _node(node)
		, _error(error)
		, _message(
			  "Config error at " + std::to_string(GetLine() + 1) + ":"
			  + std::to_string(GetColumn() + 1) + ": " + GetError())
	{
	}

	/**
	 * @brief Get the error message
	 *
	 * @return const char*
	 */
	const char* what() const noexcept { return _message.c_str(); }

	/**
	 * @brief Get the base error description
	 *
	 * @return const std::string&
	 */
	const std::string& GetError() const noexcept { return _error; }

	/**
	 * @brief Get the line where the error happened
	 *
	 * @return int  Line starting from 0
	 */
	int GetLine() const noexcept { return _node.Mark().line; }

	/**
	 * @brief Get the column where the error happened
	 *
	 * @return int  Column in the line starting from 0
	 */
	int GetColumn() const noexcept { return _node.Mark().column; }

	/**
	 * @brief Print a pretty error message
	 *
	 * @param configFilename  The config file
	 * @param output          The output stream to print the message to
	 * @param linesAround     How many of the lines of the configuration around the error to show,
	 * negative = none
	 */
	void PrintPrettyError(
		const std::string& configFilename,
		std::ostream& output,
		int linesAround = 5) const;

private:
	YAML::Node _node;
	std::string _error;
	std::string _message;
};

/**
 * @brief Get the type of the yaml node in string form, for error messages
 *
 * @param node  The node
 * @return std::string
 */
static inline std::string NodeTypeToStr(const YAML::Node& node)
{
	if (node.IsMap()) {
		return "map";
	} else if (node.IsScalar()) {
		return "scalar";
	} else if (node.IsSequence()) {
		return "sequence";
	} else if (node.IsNull()) {
		return "null";
	} else {
		return "undefined";
	}
}

template <typename T>
std::optional<T> ParseValue(const std::string& s)
{
	T value;
	auto [endPtr, errCode] = std::from_chars(s.data(), s.data() + s.size(), value);
	if (errCode == std::errc() && endPtr == s.data() + s.size()) {
		return value;
	} else {
		return std::nullopt;
	}
}

template <typename ParserT, typename ValueT>
std::vector<ValueT> ParseMany(const YAML::Node& node)
{
	ExpectSequence(node);
	std::vector<ValueT> values;
	for (const auto& subnode : node) {
		values.push_back(ParserT(subnode));
	}
	return values;
}

template <typename T>
std::vector<T> ParseMany(const YAML::Node& node)
{
	return ParseMany<T, T>(node);
}

template <typename T>
std::vector<T> ParseOneOrMany(const YAML::Node& node)
{
	if (node.IsScalar()) {
		return {T(node)};
	} else if (node.IsSequence()) {
		return ParseMany<T>(node);
	} else {
		throw ConfigError(node, "expected sequence or scalar, but got " + NodeTypeToStr(node));
	}
}

void PrintError(const ConfigError& error, const std::string& filename);

/**
 * @brief Get the keys of a map
 *
 * @param map The map to get the keys of
 * @return A vector of the key values
 */
template <typename K, typename V>
std::vector<K> KeysOfMap(const std::map<K, V>& map)
{
	std::vector<K> keys;
	keys.reserve(map.size());
	for (const auto& [k, _] : map) {
		keys.emplace_back(k);
	}
	return keys;
}

/**
 * @brief Join a list of a string values into a single string
 *
 * @param values The values to join together
 * @param delimiter The delimiter to join the values by
 */
std::string StringJoin(const std::vector<std::string>& values, const std::string& delimiter);

} // namespace config
} // namespace generator
