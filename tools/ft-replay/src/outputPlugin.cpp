/**
 * @file
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Output plugin interface implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "outputPlugin.hpp"

#include <algorithm>
#include <map>
#include <string>

namespace replay {

std::map<std::string, std::string> OutputPlugin::SplitArguments(const std::string& args) const
{
	std::map<std::string, std::string> argMap;
	std::string argString = args;
	size_t start = 0;
	size_t end = 0;

	auto noSpaceEnd = std::remove(argString.begin(), argString.end(), ' ');
	argString.erase(noSpaceEnd, argString.end());

	while (end != std::string::npos) {
		end = argString.find(',', start);
		std::string tmp = argString.substr(start, end - start);

		size_t mid = tmp.find('=');
		if (mid == std::string::npos || mid + 1 >= tmp.size() || tmp.substr(0, mid).size() == 0) {
			throw std::invalid_argument("OutputPlugin::SplitArguments() has failed");
		}

		auto ret = argMap.emplace(tmp.substr(0, mid), tmp.substr(mid + 1));
		if (!ret.second) {
			throw std::invalid_argument("OutputPlugin::SplitArguments() has failed");
		}
		start = end + 1;
	}

	return argMap;
}

Offloads OutputPlugin::ConfigureOffloads([[maybe_unused]] const OffloadRequests& offloads)
{
	return 0;
}

} // namespace replay
