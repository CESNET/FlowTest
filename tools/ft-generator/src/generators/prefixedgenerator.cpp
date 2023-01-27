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

PrefixedGenerator::PrefixedGenerator(
	const std::bitset<128>& prefix,
	uint8_t prefixLen,
	uint8_t totalLen,
	const std::bitset<128>& seed) :
	_prefix((prefix << (128 - prefixLen)) >> (128 - prefixLen)),
	_prefixLen(prefixLen),
	_lfsr(Lfsr(totalLen - prefixLen, seed))
{
	if (prefixLen >= totalLen) {
		throw std::invalid_argument("invalid PrefixedGenerator prefix length");
	}
}

std::bitset<128> PrefixedGenerator::Generate()
{
	return _prefix | (_lfsr.Next() << _prefixLen);
}

} // namespace generator
