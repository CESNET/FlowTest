/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generators of addresses used in flows
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "addressgenerators.h"

namespace generator {

template <std::size_t N>
static std::array<uint8_t, N> nextValue(std::array<uint8_t, N> octets) {
	for (int i = static_cast<int>(N) - 1; i >= 0; i--) {
		if (octets[i] == 255) {
			octets[i] = 0;
		} else {
			octets[i] += 1;
			break;
		}
	}
	return octets;
}

MacAddress MacAddressGenerator::Generate() {
	MacAddress addr(_currentAddr.data());
	_currentAddr = nextValue(_currentAddr);
	return addr;
}

IPv4Address IPv4AddressGenerator::Generate() {
	IPv4Address addr(_currentAddr.data());
	_currentAddr = nextValue(_currentAddr);
	return addr;
}

IPv6Address IPv6AddressGenerator::Generate() {
	IPv6Address addr(_currentAddr.data());
	_currentAddr = nextValue(_currentAddr);
	return addr;
}

} // namespace generator
