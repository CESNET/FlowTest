/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary logger functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "logger.h"

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using namespace std;

namespace ft {

void LoggerInit()
{
	spdlog::cfg::load_env_levels();
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %n: %v");
}

std::shared_ptr<spdlog::logger> LoggerGet(std::string_view name)
{
	const string tmp {name};
	auto logger = spdlog::get(tmp);

	if (logger) {
		// Logger already exists
		return logger;
	}

	return spdlog::stdout_color_mt(tmp);
}

} // namespace ft
