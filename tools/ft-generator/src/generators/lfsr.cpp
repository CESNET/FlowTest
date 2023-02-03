/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief LFSR generator implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lfsr.h"

#include <stdexcept>

namespace generator {

/**
 * LFSR tap configurations for the maximum period on the specified number of bytes in XNOR form
 * index in the array corresponds to the number of bits
 * taken from https://docs.xilinx.com/v/u/en-US/xapp052
 *
 * Note: The values are modified to be zero indexed as opposed to one indexed as in the linked pdf
 */
static const std::vector<std::vector<uint64_t>> LFSR_TAPS = {
	{},
	{0},
	{1},
	{2, 1},
	{3, 2},
	{4, 2},
	{5, 4},
	{6, 5},
	{7, 5, 4, 3},
	{8, 4},
	{9, 6},
	{10, 8},
	{11, 5, 3, 0},
	{12, 3, 2, 0},
	{13, 4, 2, 0},
	{14, 13},
	{15, 14, 12, 3},
	{16, 13},
	{17, 10},
	{18, 5, 1, 0},
	{19, 16},
	{20, 18},
	{21, 20},
	{22, 17},
	{23, 22, 21, 16},
	{24, 21},
	{25, 5, 1, 0},
	{26, 4, 1, 0},
	{27, 24},
	{28, 26},
	{29, 5, 3, 0},
	{30, 27},
	{31, 21, 1, 0},
	{32, 19},
	{33, 26, 1, 0},
	{34, 32},
	{35, 24},
	{36, 4, 3, 2, 1, 0},
	{37, 5, 4, 0},
	{38, 34},
	{39, 37, 20, 18},
	{40, 37},
	{41, 40, 19, 18},
	{42, 41, 37, 36},
	{43, 42, 17, 16},
	{44, 43, 41, 40},
	{45, 44, 25, 24},
	{46, 41},
	{47, 46, 20, 19},
	{48, 39},
	{49, 48, 23, 22},
	{50, 49, 35, 34},
	{51, 48},
	{52, 51, 37, 36},
	{53, 52, 17, 16},
	{54, 30},
	{55, 54, 34, 33},
	{56, 49},
	{57, 38},
	{58, 57, 37, 36},
	{59, 58},
	{60, 59, 45, 44},
	{61, 60, 5, 4},
	{62, 61},
	{63, 62, 60, 59},
	{64, 46},
	{65, 64, 56, 55},
	{66, 65, 57, 56},
	{67, 58},
	{68, 66, 41, 39},
	{69, 68, 54, 53},
	{70, 64},
	{71, 65, 24, 18},
	{72, 47},
	{73, 72, 58, 57},
	{74, 73, 64, 63},
	{75, 74, 40, 39},
	{76, 75, 46, 45},
	{77, 76, 58, 57},
	{78, 69},
	{79, 78, 42, 41},
	{80, 76},
	{81, 78, 46, 43},
	{82, 81, 37, 36},
	{83, 70},
	{84, 83, 57, 56},
	{85, 84, 73, 72},
	{86, 73},
	{87, 86, 16, 15},
	{88, 50},
	{89, 88, 71, 70},
	{90, 89, 7, 6},
	{91, 90, 79, 78},
	{92, 90},
	{93, 72},
	{94, 83},
	{95, 93, 48, 46},
	{96, 90},
	{97, 86},
	{98, 96, 53, 51},
	{99, 62},
	{100, 99, 94, 93},
	{101, 100, 35, 34},
	{102, 93},
	{103, 102, 93, 92},
	{104, 88},
	{105, 90},
	{106, 104, 43, 41},
	{107, 76},
	{108, 107, 102, 101},
	{109, 108, 97, 96},
	{110, 100},
	{111, 109, 68, 66},
	{112, 103},
	{113, 112, 32, 31},
	{114, 113, 100, 99},
	{115, 114, 45, 44},
	{116, 114, 98, 96},
	{117, 84},
	{118, 110},
	{119, 112, 8, 1},
	{120, 102},
	{121, 120, 62, 61},
	{122, 120},
	{123, 86},
	{124, 123, 17, 16},
	{125, 124, 89, 88},
	{126, 125},
	{127, 125, 100, 98},
};

Lfsr::Lfsr(unsigned int nBits, const std::bitset<128>& seed)
{
	if (nBits < 1 || nBits > 128) {
		throw std::invalid_argument("number of bits must be between 1 and 128 inclusive");
	}
	_nBits = nBits;
	_taps = LFSR_TAPS[nBits];
	_state = seed;
}

std::bitset<128> Lfsr::Next()
{
	for (unsigned int i = 0; i < _nBits; i++) {
		Shift();
	}
	return _state;
}

void Lfsr::Shift()
{
	uint8_t bit = _state[_taps[0]];
	for (unsigned int i = 1; i < _taps.size(); i++) {
		bit = !(bit ^ _state[_taps[i]]);
	}
	_state >>= 1;
	_state[_nBits - 1] = bit;
}

} // namespace generator
