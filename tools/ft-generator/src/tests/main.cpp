/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Main entry point for doctest test runner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define DOCTEST_CONFIG_IMPLEMENT

#include "../randomgenerator.h"
#include "logger.h"

#include <doctest.h>

int main(int argc, char** argv)
{
	ft::LoggerInit();

	uint64_t seed = std::time(nullptr);
	generator::RandomGenerator::InitInstance(seed);

	doctest::Context context;
	context.applyCommandLine(argc, argv);
	return context.run();
}
