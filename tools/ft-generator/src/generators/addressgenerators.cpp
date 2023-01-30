/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Generators of addresses used in flows
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "addressgenerators.h"
#include "../randomgenerator.h"
#include "common.h"

#include <cassert>
#include <iostream>

namespace generator {

template <typename RangeT, typename GeneratorT, typename AddressT>
static MultiRangeGenerator<GeneratorT, AddressT> Make(const std::vector<RangeT>& configs)
{
	std::vector<GeneratorT> generators;
	for (const auto& range : configs) {
		generators.emplace_back(range.GetBaseAddr(), range.GetPrefixLen(), generateSeed());
	}
	if (configs.empty()) {
		generators.emplace_back(AddressT::Zero, 0, generateSeed());
	}
	return MultiRangeGenerator<GeneratorT, AddressT>(std::move(generators));
}

AddressGenerators::AddressGenerators(
	const std::vector<config::IPv4AddressRange>& ipv4Config,
	const std::vector<config::IPv6AddressRange>& ipv6Config,
	const std::vector<config::MacAddressRange>& macConfig)
	: _ipv4(Make<config::IPv4AddressRange, IPv4AddressGenerator, IPv4Address>(ipv4Config))
	, _ipv6(Make<config::IPv6AddressRange, IPv6AddressGenerator, IPv6Address>(ipv6Config))
	, _mac(Make<config::MacAddressRange, MacAddressGenerator, MacAddress>(macConfig))
{
}

MacAddress AddressGenerators::GenerateMac()
{
	return _mac.Generate();
}

IPv4Address AddressGenerators::GenerateIPv4()
{
	return _ipv4.Generate();
}

IPv6Address AddressGenerators::GenerateIPv6()
{
	return _ipv6.Generate();
}

} // namespace generator
