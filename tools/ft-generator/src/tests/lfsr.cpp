/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for LFSR
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../generators/lfsr.h"

#include <doctest.h>

#include <unordered_set>

using namespace generator;

TEST_SUITE_BEGIN("Lfsr");

TEST_CASE("0 bit LFSR is valid and should have empty state vector")
{
	Lfsr lfsr(0);
	CHECK(lfsr.GetState().empty());
	lfsr.Next();
	CHECK(lfsr.GetState().empty());
}

TEST_CASE("period of LFSR of N bits should be 2^N - 1")
{
	// Check periods of up to 20 bits. Higher lengths are too infeasible to test.
	for (int i = 1; i <= 20; i++) {
		int nBits = i;
		Lfsr lfsr(nBits);
		std::unordered_set<uint64_t> values;

		while (true) {
			uint64_t value = 0;
			for (auto b : lfsr.GetState()) {
				value <<= 1;
				if (b) {
					value |= 1;
				}
			}
			if (values.find(value) != values.end()) {
				break;
			}
			values.emplace(value);
			lfsr.Next();
		}

		CHECK(values.size() == (1 << nBits) - 1);
	}
}

TEST_SUITE_END();
