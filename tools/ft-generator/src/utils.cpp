/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief General utility functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils.h"

#include <cstdint>

namespace generator {

void SetBit(unsigned int position, bool bitValue, uint8_t* bytes)
{
	unsigned int byteIdx = position / 8;
	unsigned int bitIdx = 7 - (position % 8);
	bytes[byteIdx] &= ~(uint8_t(1) << bitIdx);
	bytes[byteIdx] |= uint8_t(bitValue) << bitIdx;
}

} // namespace generator
