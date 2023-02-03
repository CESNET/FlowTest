/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Common functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../randomgenerator.h"

#include <bitset>
#include <cstdint>

namespace generator {

/**
 * @brief Copy the specified number of bytes from a bitset to a byte array.
 *
 * If the provided length is more than the length of the bitset (128), the behavior is undefined.
 */
void BitsetToBytes(std::bitset<128> bitset, uint8_t* bytes, int length);

/**
 * @brief Make bitset from an array of bytes.
 *
 * If there is less than 128 bits, the remainder of the bitset is filled with zeroes.
 */
std::bitset<128> BytesToBitset(const uint8_t* bytes, int length);

/**
 * @brief Generate a random 128 bit seed
 */
std::bitset<128> GenerateSeed();

} // namespace generator
