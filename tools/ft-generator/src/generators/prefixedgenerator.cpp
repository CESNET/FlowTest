/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Prefixed generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "prefixedgenerator.h"

#include <stdexcept>

namespace generator {

PrefixedGenerator::PrefixedGenerator(const std::vector<uint8_t>& base, unsigned int prefixLen)
	: _base(base)
	, _prefixLen(prefixLen)
	, _totalLen(base.size() * 8)
	, _lfsr(_totalLen - _prefixLen)
{
	if (_prefixLen > _totalLen) {
		throw std::invalid_argument("invalid PrefixedGenerator prefix length");
	}
}

std::vector<uint8_t> PrefixedGenerator::Generate()
{
	std::vector<uint8_t> result = _base;
	const std::vector<bool>& state = _lfsr.GetState();
	auto it = state.begin();
	for (unsigned int n = _prefixLen; n < _totalLen && it != state.end(); n++, it++) {
		bool bit = *it;
		unsigned int byteIdx = n / 8;
		unsigned int bitIdx = 7 - (n % 8);
		result[byteIdx] &= ~(uint8_t(1) << bitIdx);
		result[byteIdx] |= uint8_t(bit) << bitIdx;
	}
	_lfsr.Next();
	return result;
}

} // namespace generator
