/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"

namespace generator {

static uint8_t ReverseBits(uint8_t b)
{
	// Reverse the bit order in a byte
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

void BitsetToBytes(std::bitset<128> bitset, uint8_t* bytes, int length)
{
	for (int i = 0; i < length; i++) {
		uint8_t b = (bitset & std::bitset<128>(0xFF)).to_ulong();
		b = ReverseBits(b);
		bytes[i] = b;
		bitset >>= 8;
	}
}

std::bitset<128> BytesToBitset(const uint8_t* bytes, int length)
{
	std::bitset<128> bitset {0};
	for (int i = 0; i < length; i++) {
		uint8_t b = bytes[length - 1 - i];
		b = ReverseBits(b);
		bitset = (bitset << 8) | std::bitset<128>(b);
	}
	return bitset;
}

std::bitset<128> GenerateSeed()
{
	auto& rng = RandomGenerator::GetInstance();
	return (std::bitset<128>(rng.RandomUInt()) << 64) | std::bitset<128>(rng.RandomUInt());
}

} // namespace generator
