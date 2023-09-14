/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for ranodm number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../randomgenerator.h"

#include <doctest.h>

#include <algorithm>
#include <numeric>

using namespace generator;

TEST_SUITE_BEGIN("RandomGenerator");

TEST_CASE("randomly distributed values sum to the desired amount")
{
	auto& g = RandomGenerator::GetInstance();
	auto vals = g.RandomlyDistribute(1000, 100, 2, 20);

	CHECK(vals.size() == 100);
	CHECK(std::accumulate(vals.begin(), vals.end(), uint64_t(0)) == 1000);
	CHECK(std::all_of(vals.begin(), vals.end(), [](auto v) { return v >= 2; }));
	CHECK(std::all_of(vals.begin(), vals.end(), [](auto v) { return v <= 20; }));
}

TEST_CASE("randomly distributed values sum to the desired amount (2)")
{
	auto& g = RandomGenerator::GetInstance();
	auto vals = g.RandomlyDistribute(436, 2, 14, 268);

	CHECK(vals.size() == 2);
	CHECK(std::accumulate(vals.begin(), vals.end(), uint64_t(0)) == 436);
	CHECK(std::all_of(vals.begin(), vals.end(), [](auto v) { return v >= 14; }));
	CHECK(std::all_of(vals.begin(), vals.end(), [](auto v) { return v <= 268; }));
}

TEST_SUITE_END();
