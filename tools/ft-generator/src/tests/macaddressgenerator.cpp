/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for mac address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../generators/macaddressgenerator.h"

#include <doctest.h>

#include <string>
#include <unordered_set>

using namespace generator;

TEST_SUITE_BEGIN("MacAddressGenerator");

TEST_CASE("prefix should match")
{
	MacAddressGenerator g("a2:b3:c0:00:00:00", 20);
	const auto prefixStrLen = std::string("xx:xx:x").size();

	for (int i = 0; i < 1000; i++) {
		auto mac = g.Generate();
		CHECK(mac.toString().substr(0, prefixStrLen) == "a2:b3:c");
	}
}

TEST_CASE("addresses with LSB=1 in first octet should not be generated")
{
	MacAddressGenerator g(MacAddress::Zero, 0);

	for (int i = 0; i < 1000; i++) {
		auto mac = g.Generate();
		CHECK((mac.getRawData()[0] & 1) == 0);
	}
}

TEST_CASE("addresses with LSB=1 in first octet will be generated if prefix explicitly specifies it")
{
	MacAddressGenerator g("ff:ff:f0:00:00:00", 20);
	const auto prefixStrLen = std::string("xx:xx:x").size();

	for (int i = 0; i < 1000; i++) {
		auto mac = g.Generate();
		CHECK(mac.toString().substr(0, prefixStrLen) == "ff:ff:f");
	}
}

TEST_CASE("period with N address bits should be 2^N")
{
	int prefixLen = 32;
	int remBits = 48 - prefixLen;
	MacAddressGenerator g(MacAddress::Zero, prefixLen);

	std::unordered_set<std::string> seen;

	while (true) {
		auto mac = g.Generate();
		auto macStr = mac.toString();
		if (seen.find(macStr) != seen.end()) {
			break;
		}
		seen.emplace(macStr);
	}

	std::size_t expectedPeriod = (1 << remBits);
	CHECK(seen.size() == expectedPeriod);
}

TEST_SUITE_END();
