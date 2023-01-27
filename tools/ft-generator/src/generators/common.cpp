/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

namespace generator {

void bitsetToBytes(std::bitset<128> bitset, uint8_t* bytes, int length)
{
	for (int i = 0; i < length; i++) {
		bytes[i] = (bitset & std::bitset<128>(0xFF)).to_ulong();
		bitset >>= 8;
	}
}

std::bitset<128> bytesToBitset(const uint8_t* bytes, int length)
{
	std::bitset<128> bitset{0};
	for (int i = 0; i < length; i++) {
		bitset = (bitset << 8) | std::bitset<128>(bytes[length - 1 - i]);
	}
	return bitset;
}

std::bitset<128> generateSeed()
{
	auto& rng = RandomGenerator::GetInstance();
	return (std::bitset<128>(rng.RandomUInt()) << 64) | std::bitset<128>(rng.RandomUInt());
}

} // namespace generator
