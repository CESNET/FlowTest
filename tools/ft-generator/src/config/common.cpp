/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Module of common functions for parsing configurations
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace generator {
namespace config {

std::vector<std::string> StringSplit(const std::string& s, const std::string& delimiter)
{
	std::size_t pos = 0;
	std::size_t nextPos;
	std::vector<std::string> pieces;
	while ((nextPos = s.find(delimiter, pos)) != std::string::npos) {
		pieces.emplace_back(s.begin() + pos, s.begin() + nextPos);
		pos = nextPos + delimiter.size();
	}
	pieces.emplace_back(s.begin() + pos, s.end());
	return pieces;
}

std::string StringStrip(std::string s)
{
	auto isNotWhitespace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), isNotWhitespace));
	s.erase(std::find_if(s.rbegin(), s.rend(), isNotWhitespace).base(), s.end());
	return s;
}

void ExpectSequence(const YAML::Node& node)
{
	if (!node.IsSequence()) {
		throw ConfigError(node, "expected a sequence, but got " + NodeTypeToStr(node));
	}
}

void ExpectMap(const YAML::Node& node)
{
	if (!node.IsMap()) {
		throw ConfigError(node, "expected a map, but got " + NodeTypeToStr(node));
	}
}

void ExpectScalar(const YAML::Node& node)
{
	if (!node.IsScalar()) {
		throw ConfigError(node, "expected a scalar, but got " + NodeTypeToStr(node));
	}
}

void ExpectKey(const YAML::Node& node, const std::string& key)
{
	if (!node[key].IsDefined()) {
		throw ConfigError(node, "expected a missing key \"" + key + "\"");
	}
}

std::string AsScalar(const YAML::Node& node)
{
	ExpectScalar(node);
	return node.as<std::string>();
}

void CheckAllowedKeys(const YAML::Node& node, const std::vector<std::string>& allowedKeys)
{
	for (const auto& kv : node) {
		const auto& key = kv.first.as<std::string>();
		if (std::find(allowedKeys.begin(), allowedKeys.end(), key) == allowedKeys.end()) {
			std::string allowedKeysStr = "";
			for (size_t i = 0; i < allowedKeys.size(); i++) {
				allowedKeysStr
					+= "\"" + allowedKeys[i] + "\"" + (i < allowedKeys.size() - 1 ? ", " : "");
			}
			throw ConfigError(
				kv.first,
				"unexpected key \"" + key + "\", allowed keys are " + allowedKeysStr);
		}
	}
}

double ParseProbability(const YAML::Node& node)
{
	const std::string& s = AsScalar(node);
	double result;

	if (s.empty()) {
		throw ConfigError(node, "expected probability as double or percentage");
	}

	if (s[s.size() - 1] == '%') {
		auto num = ParseValue<double>(std::string(s.begin(), s.end() - 1));
		if (!num) {
			throw ConfigError(node, "expected probability as double or percentage");
		}
		result = *num / 100.0;

	} else {
		auto num = ParseValue<double>(s);
		if (!num) {
			throw ConfigError(node, "expected probability as double or percentage");
		}
		result = *num;
	}

	if (result < 0.0 || result > 1.0) {
		throw ConfigError(node, "invalid probability value, outside of the range <0, 1>");
	}

	return result;
}

void ConfigError::PrintPrettyError(
	const std::string& configFilename,
	std::ostream& output,
	int linesAround) const
{
	output << what() << std::endl;
	std::ifstream file(configFilename);
	if (!file) {
		return;
	}
	std::string line;
	int lineNum = 0;
	int errLineNum = GetLine();
	int errColNum = GetColumn();
	int numW = std::to_string(errLineNum + linesAround).size();
	while (std::getline(file, line)) {
		if (std::abs(lineNum - errLineNum) <= linesAround) {
			if (lineNum == errLineNum) {
				output << "-> ";
			} else {
				output << "   ";
			}
			output << std::setw(numW) << (lineNum + 1) << " |";
			output << line << std::endl;
			if (lineNum == errLineNum) {
				output << "     ";
				for (int i = 0; i < numW; i++) {
					output << " ";
				}
				for (int i = 0; i < errColNum; i++) {
					output << (line[errColNum] == '\t' ? '\t' : ' ');
				}
				output << "^";
				output << std::endl;
			}
		}
		lineNum++;
	}
}

template <>
std::optional<double> ParseValue(const std::string& s)
{
	double value;
	size_t endIndex;
	try {
		value = std::stod(s, &endIndex);
	} catch (...) {
		return std::nullopt;
	}
	if (endIndex != s.size()) {
		return std::nullopt;
	}
	return value;
}

} // namespace config
} // namespace generator
