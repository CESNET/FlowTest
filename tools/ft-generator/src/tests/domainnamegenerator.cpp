/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for domain name generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../domainnamegenerator.h"

#include <doctest.h>

#include <regex>

using namespace generator;

TEST_SUITE_BEGIN("DomainNameGenerator");

TEST_CASE("generated domain names are the specified length")
{
	auto& g = DomainNameGenerator::GetInstance();

	for (int i = 4; i < 255; i++) {
		auto domain = g.Generate(i);
		CAPTURE(domain);
		CHECK(domain.size() == i);
	}
}

TEST_CASE("generated domain names are the correct format")
{
	auto& g = DomainNameGenerator::GetInstance();

	std::regex re("(([a-zA-Z0-9]+-)*[a-zA-Z0-9]+\\.)+[a-zA-Z0-9]+");
	for (int i = 4; i < 255; i++) {
		auto domain = g.Generate(i);
		CAPTURE(domain);
		CHECK(std::regex_match(domain, re));
	}
}

TEST_SUITE_END();
