/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Normalized flow identifier
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "normalizedflowidentifier.h"

#include <sstream>

namespace generator {

NormalizedFlowIdentifier::NormalizedFlowIdentifier(
	const pcpp::IPAddress& srcIp,
	const pcpp::IPAddress& dstIp,
	uint16_t srcPort,
	uint16_t dstPort,
	L4Protocol l4Proto)
	: _l4Proto(l4Proto)
{
	/**
	 * We want these identifiers to be direction-invariant. We accomplish this by ensuring that the
	 * (address, port) pairs are always stored in the same order. ip1 is always the address with the
	 * "lower" value of the two, ip2 is the address with the "higher" value of the two. If both
	 * addresses are the same, the order is decided by the ports.
	 */
	if (srcIp < dstIp || (srcIp == dstIp && srcPort < dstPort)) {
		_ip1 = srcIp;
		_ip2 = dstIp;
		_port1 = srcPort;
		_port2 = dstPort;
	} else {
		_ip1 = dstIp;
		_ip2 = srcIp;
		_port1 = dstPort;
		_port2 = srcPort;
	}
}

bool NormalizedFlowIdentifier::operator==(const NormalizedFlowIdentifier& other) const
{
	return _ip1 == other._ip1 && _ip2 == other._ip2 && _port1 == other._port1
		&& _port2 == other._port2 && _l4Proto == other._l4Proto;
}

bool NormalizedFlowIdentifier::operator!=(const NormalizedFlowIdentifier& other) const
{
	return !(*this == other);
}

bool NormalizedFlowIdentifier::operator<(const NormalizedFlowIdentifier& other) const
{
	/**
	 * We want these flow identifiers to be orderable so we can use them in a set.
	 *
	 * We accomplish this by comparing their ip1, port1, ip2, port2, proto elements in this order.
	 *
	 * I.e. we compare ip1 and other.ip1, if they aren't the same, we can use them to define our
	 * order. If they are the same, we move onto the next one. If all the elements are the same,
	 * then both the objects are equal and we return false.
	 */

	if (_ip1 != other._ip1) {
		return _ip1 < other._ip1;
	}

	if (_port1 != other._port1) {
		return _port1 < other._port1;
	}

	if (_ip2 != other._ip2) {
		return _ip2 < other._ip2;
	}

	if (_port2 != other._port2) {
		return _port2 < other._port2;
	}

	if (_l4Proto != other._l4Proto) {
		return _l4Proto < other._l4Proto;
	}

	return false;
}

std::string NormalizedFlowIdentifier::ToString() const
{
	auto ipToStr = [](const pcpp::IPAddress& ip) {
		// Surround IPv6 addresses in `[` `]` for printing with port
		return ip.isIPv6() ? ("[" + ip.toString() + "]") : ip.toString();
	};

	std::stringstream ss;
	ss << ipToStr(_ip1) << ":" << _port1;
	ss << " <-> ";
	ss << ipToStr(_ip2) << ":" << _port2;
	ss << " ";
	ss << L4ProtocolToString(_l4Proto);
	return ss.str();
}

} // namespace generator
