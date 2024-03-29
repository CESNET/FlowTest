/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief LFSR generator implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lfsr.h"
#include "../randomgenerator.h"

#include <stdexcept>
#include <vector>

namespace generator {

/**
 * List of primitive polynomials of degrees 1-128 in GF(2)
 *
 * These let us achieve maximum (fibonacci type) LFSR period 2^n - 1
 *
 * In other words, these are the indexes of the values or "taps" of the shift
 * register that should be XORed together in each round to produce the next
 * input bit
 *
 * Source: A Table of Primitive Binary Polynomials, Miodrag Zivkovic
 * http://poincare.matf.bg.ac.rs/~ezivkovm/publications/primpol1.pdf
 */
static const std::vector<std::vector<int>> PRIMITIVE_POLYNOMIALS {
	{},
	{1},
	{1, 2},
	{1, 3},
	{1, 4},
	{1, 2, 3, 5},
	{1, 4, 5, 6},
	{1, 2, 3, 4, 5, 7},
	{2, 4, 5, 6, 7, 8},
	{2, 3, 6, 7, 8, 9},
	{1, 2, 5, 6, 7, 10},
	{1, 2, 5, 7, 9, 11},
	{2, 6, 8, 9, 10, 12},
	{1, 2, 5, 10, 12, 13},
	{1, 3, 4, 5, 11, 14},
	{5, 6, 7, 8, 13, 15},
	{2, 9, 12, 13, 14, 16},
	{2, 5, 6, 8, 13, 17},
	{1, 4, 7, 8, 10, 18},
	{6, 12, 13, 16, 18, 19},
	{1, 10, 14, 16, 18, 20},
	{6, 8, 14, 18, 19, 21},
	{2, 4, 9, 14, 21, 22},
	{5, 11, 12, 13, 17, 23},
	{3, 6, 7, 16, 23, 24},
	{7, 10, 13, 15, 23, 25},
	{1, 6, 15, 17, 24, 26},
	{6, 11, 17, 18, 19, 27},
	{5, 11, 21, 24, 27, 28},
	{3, 11, 15, 16, 22, 29},
	{11, 12, 24, 28, 29, 30},
	{1, 8, 10, 14, 16, 31},
	{1, 3, 12, 17, 30, 32},
	{1, 8, 17, 19, 32, 33},
	{4, 7, 14, 20, 31, 34},
	{2, 21, 23, 31, 32, 35},
	{6, 17, 25, 26, 28, 36},
	{3, 21, 30, 31, 33, 37},
	{6, 9, 11, 20, 36, 38},
	{2, 13, 15, 36, 38, 39},
	{6, 7, 18, 28, 36, 40},
	{11, 12, 20, 32, 40, 41},
	{1, 8, 14, 24, 27, 42},
	{8, 25, 30, 32, 35, 43},
	{5, 16, 25, 40, 43, 44},
	{14, 15, 23, 27, 33, 45},
	{21, 23, 24, 40, 44, 46},
	{5, 17, 19, 32, 42, 47},
	{5, 12, 27, 29, 43, 48},
	{8, 39, 41, 42, 45, 49},
	{5, 6, 16, 21, 36, 50},
	{12, 15, 22, 24, 25, 51},
	{1, 2, 16, 25, 50, 52},
	{4, 18, 29, 37, 51, 53},
	{9, 10, 23, 24, 34, 54},
	{16, 23, 44, 45, 51, 55},
	{5, 20, 28, 38, 45, 56},
	{4, 5, 31, 40, 50, 57},
	{23, 32, 37, 54, 55, 58},
	{21, 22, 34, 45, 53, 59},
	{12, 13, 19, 31, 48, 60},
	{33, 38, 47, 52, 59, 61},
	{2, 9, 16, 18, 48, 62},
	{5, 8, 18, 22, 60, 63},
	{23, 28, 31, 56, 61, 64},
	{8, 10, 15, 43, 60, 65},
	{4, 7, 8, 23, 50, 66},
	{25, 26, 28, 44, 64, 67},
	{14, 29, 39, 41, 63, 68},
	{21, 22, 39, 44, 50, 69},
	{30, 34, 43, 58, 63, 70},
	{21, 30, 34, 45, 49, 71},
	{6, 10, 11, 14, 22, 72},
	{2, 12, 35, 48, 66, 73},
	{4, 17, 23, 28, 69, 74},
	{2, 21, 29, 60, 72, 75},
	{1, 17, 27, 28, 34, 76},
	{13, 25, 62, 68, 74, 77},
	{5, 29, 40, 53, 73, 78},
	{28, 33, 39, 56, 57, 79},
	{10, 37, 50, 51, 70, 80},
	{1, 27, 28, 48, 63, 81},
	{43, 44, 53, 66, 79, 82},
	{25, 27, 42, 47, 67, 83},
	{15, 30, 49, 62, 82, 84},
	{17, 22, 27, 44, 78, 85},
	{32, 47, 56, 65, 78, 86},
	{24, 52, 65, 68, 85, 87},
	{33, 46, 51, 54, 86, 88},
	{18, 21, 31, 68, 81, 89},
	{45, 62, 64, 74, 82, 90},
	{1, 44, 58, 78, 83, 91},
	{42, 47, 65, 74, 76, 92},
	{12, 66, 73, 80, 83, 93},
	{2, 14, 18, 28, 43, 94},
	{5, 17, 40, 90, 92, 95},
	{4, 10, 11, 14, 57, 96},
	{5, 6, 28, 53, 82, 97},
	{5, 34, 35, 41, 75, 98},
	{4, 9, 28, 43, 84, 99},
	{16, 22, 34, 77, 83, 100},
	{33, 45, 57, 86, 92, 101},
	{38, 50, 52, 65, 88, 102},
	{22, 35, 43, 67, 69, 103},
	{33, 43, 80, 81, 102, 104},
	{7, 15, 21, 40, 101, 105},
	{12, 17, 34, 78, 86, 106},
	{23, 29, 40, 84, 89, 107},
	{36, 43, 46, 62, 68, 108},
	{3, 69, 74, 95, 100, 109},
	{7, 17, 30, 70, 72, 110},
	{19, 54, 71, 101, 102, 111},
	{63, 71, 87, 109, 111, 112},
	{13, 38, 48, 92, 109, 113},
	{2, 38, 62, 74, 79, 114},
	{17, 21, 47, 58, 98, 115},
	{4, 11, 12, 43, 105, 116},
	{4, 53, 70, 74, 104, 117},
	{29, 37, 45, 59, 109, 118},
	{20, 43, 92, 111, 116, 119},
	{70, 71, 77, 82, 87, 120},
	{8, 25, 105, 115, 116, 121},
	{93, 98, 100, 109, 119, 122},
	{4, 14, 18, 21, 121, 123},
	{48, 60, 72, 74, 107, 124},
	{9, 24, 39, 57, 108, 125},
	{51, 64, 70, 78, 81, 126},
	{31, 38, 67, 68, 97, 127},
	{36, 38, 45, 57, 95, 128},
};

Lfsr::Lfsr(unsigned int nBits)
	: _zeroState(nBits, 0)
{
	if (nBits >= PRIMITIVE_POLYNOMIALS.size()) {
		throw std::invalid_argument(
			"number of bits must be between 0 and "
			+ std::to_string(PRIMITIVE_POLYNOMIALS.size() - 1) + " inclusive");
	}

	_taps = PRIMITIVE_POLYNOMIALS[nBits];
	_state.resize(nBits);

	if (_state.empty()) {
		return;
	}

	do {
		auto& rng = RandomGenerator::GetInstance();
		for (unsigned int i = 0; i < _state.size(); i++) {
			_state[i] = rng.RandomUInt(0, 1);
		}
		// All zeros is an invalid initial state
	} while (_state == _zeroState);

	_initState = _state;
}

void Lfsr::Next()
{
	if (_state.empty()) {
		return;
	}

	// The state that follows the zero state is the initial state, as we have
	// "manually" set the zero state as described below.
	if (_state == _zeroState) {
		_state = _initState;
		return;
	}

	bool in = false;
	for (auto tap : _taps) {
		in ^= _state[tap - 1];
	}
	_state.pop_back();
	_state.insert(_state.begin(), in);

	// We cannot ever get the all zero state from normal LFSR operation. To
	// circumvent this, set this state "manually" after looping over and
	// reaching the initial state again.
	if (_state == _initState) {
		_state = _zeroState;
	}
}

} // namespace generator
