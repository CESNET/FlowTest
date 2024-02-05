/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Module of common functions for parsing configurations
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "../utils.h"

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

bool StringCheckStripSuffix(std::string& s, const std::string& suffix)
{
	if (s.size() < suffix.size()) {
		return false;
	}

	auto startPos = s.size() - suffix.size();
	if (s.substr(startPos, suffix.size()) != suffix) {
		return false;
	}

	s = s.substr(0, startPos);
	return true;
}

void ExpectSequence(const YAML::Node& node)
{
	if (!node.IsDefined() || !node.IsSequence()) {
		throw ConfigError(node, "expected a sequence, but got " + NodeTypeToStr(node));
	}
}

void ExpectMap(const YAML::Node& node)
{
	if (!node.IsDefined() || !node.IsMap()) {
		throw ConfigError(node, "expected a map, but got " + NodeTypeToStr(node));
	}
}

void ExpectScalar(const YAML::Node& node)
{
	if (!node.IsDefined() || !node.IsScalar()) {
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
				"unexpected key \"" + key + "\", allowed keys are "
					+ allowedKeysStr); // NOLINT(performance-inefficient-string-concatenation)
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

uint64_t ParseTimeUnit(const YAML::Node& node)
{
	std::string s = AsScalar(node);
	s = StringStrip(s);

	uint64_t multiplier;

	if (StringCheckStripSuffix(s, "ns")) {
		multiplier = 1;
	} else if (StringCheckStripSuffix(s, "us")) {
		multiplier = 1'000;
	} else if (StringCheckStripSuffix(s, "ms")) {
		multiplier = 1'000'000;
	} else if (StringCheckStripSuffix(s, "s")) {
		multiplier = 1'000'000'000;
	} else {
		throw ConfigError(node, "expected time unit value with ns/us/ms/s suffix");
	}

	s = StringStrip(s);

	auto value = ParseValue<uint64_t>(s);
	if (!value) {
		throw ConfigError(node, "invalid time unit value");
	}

	return *value * multiplier;
}

uint64_t ParseSpeedUnit(const YAML::Node& node)
{
	std::string s = AsScalar(node);
	s = StringStrip(s);

	uint64_t multiplier;

	if (StringCheckStripSuffix(s, "gbps")) {
		multiplier = 1'000'000'000;
	} else if (StringCheckStripSuffix(s, "mbps")) {
		multiplier = 1'000'000;
	} else if (StringCheckStripSuffix(s, "kbps")) {
		multiplier = 1'000;
	} else if (StringCheckStripSuffix(s, "bps")) {
		multiplier = 1;
	} else {
		throw ConfigError(node, "expected speed unit value with bps/kbps/mbps/gbps suffix");
	}

	s = StringStrip(s);

	auto value = ParseValue<uint64_t>(s);
	if (!value) {
		throw ConfigError(node, "invalid speed unit value");
	}

	return OverflowCheckedMultiply(*value, multiplier * 8);
}

void ConfigError::PrintPrettyError(
	const std::string& configFilename,
	std::ostream& output,
	int linesAround) const
{
	output << what() << '\n';
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
			output << line << '\n';
			if (lineNum == errLineNum) {
				output << "     ";
				for (int i = 0; i < numW; i++) {
					output << " ";
				}
				for (int i = 0; i < errColNum; i++) {
					output << (line[errColNum] == '\t' ? '\t' : ' ');
				}
				output << "^";
				output << '\n';
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

std::string StringJoin(const std::vector<std::string>& values, const std::string& delimiter)
{
	std::string s;
	for (std::size_t i = 0; i < values.size(); i++) {
		s.append(values[i]);
		if (i < values.size() - 1) {
			s.append(delimiter);
		}
	}
	return s;
}

} // namespace config
} // namespace generator
