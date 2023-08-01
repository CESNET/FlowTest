/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for packet size generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../packetsizegenerator.h"

#include <doctest.h>

using namespace generator;

TEST_SUITE_BEGIN("PacketSizeGenerator");

TEST_CASE("generated packet size should not exceed packet size intervals (zero packets)")
{
	auto g = PacketSizeGenerator::Construct({{100, 200, 0.1}, {200, 300, 0.9}}, 0, 1000);
	g->PlanRemaining();

	for (int i = 0; i < 100; i++) {
		auto v = g->GetValue();
		CHECK(v >= 100);
		CHECK(v <= 300);
	}
}

TEST_CASE("generated packet size should not exceed packet size intervals (single packet)")
{
	auto g = PacketSizeGenerator::Construct({{100, 200, 0.1}, {200, 300, 0.9}}, 1, 1000);
	g->PlanRemaining();

	for (int i = 0; i < 10; i++) {
		auto v = g->GetValue();
		CHECK(v >= 100);
		CHECK(v <= 300);
	}
}

TEST_CASE("generated packet size should not exceed packet size intervals (small number of packets)")
{
	auto g = PacketSizeGenerator::Construct({{100, 200, 0.1}, {200, 300, 0.9}}, 10, 10 * 1000);
	g->PlanRemaining();

	for (int i = 0; i < 20; i++) {
		auto v = g->GetValue();
		CHECK(v >= 100);
		CHECK(v <= 300);
	}
}

TEST_CASE("generated packet size should not exceed packet size intervals (large number of packets)")
{
	auto g = PacketSizeGenerator::Construct({{100, 200, 0.1}, {200, 300, 0.9}}, 1000, 1000 * 1000);
	g->PlanRemaining();

	for (int i = 0; i < 2000; i++) {
		auto v = g->GetValue();
		CHECK(v >= 100);
		CHECK(v <= 300);
	}
}

TEST_SUITE_END();
