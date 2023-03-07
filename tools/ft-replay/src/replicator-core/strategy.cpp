/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator strategies implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "strategy.hpp"

#include "ipAddress.hpp"
#include "macAddress.hpp"

namespace replay {

template <typename T>
void UnitStrategy<T>::ApplyStrategy(T type)
{
	(void) type;
}

UnitIpAddConstant::UnitIpAddConstant(uint32_t constant)
	: _constant(constant)
{
}

void UnitIpAddConstant::ApplyStrategy(IpAddressView ip)
{
	ip += _constant;
}

UnitIpAddCounter::UnitIpAddCounter(uint32_t start, uint32_t step)
	: _counter(start)
	, _step(step)
{
}

void UnitIpAddCounter::ApplyStrategy(IpAddressView ip)
{
	ip += _counter;
	_counter += _step;
}

UnitMacSetAddress::UnitMacSetAddress(const MacAddress& mac)
	: _macAddress(mac)
{
}

void UnitMacSetAddress::ApplyStrategy(MacAddress mac)
{
	mac = _macAddress;
}

void LoopStrategy::ApplyStrategy(IpAddressView ip, size_t loopId)
{
	(void) ip;
	(void) loopId;
}

LoopIpAddOffset::LoopIpAddOffset(uint32_t offset)
	: _offset(offset)
{
}

void LoopIpAddOffset::ApplyStrategy(IpAddressView ip, size_t loopId)
{
	ip += _offset * loopId;
}

} // namespace replay
