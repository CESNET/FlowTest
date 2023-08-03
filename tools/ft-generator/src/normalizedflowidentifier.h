/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Normalized flow identifier
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "flowprofile.h"

#include <pcapplusplus/IpAddress.h>

#include <cstdint>
#include <string>

namespace generator {

/**
 * @brief The 5-tuple uniquely identifying a flow in a flow cache, represented in a
 *        "normalized", i.e. direction-invariant, way
 */
struct NormalizedFlowIdentifier {
	pcpp::IPAddress _ip1;
	pcpp::IPAddress _ip2;
	uint16_t _port1;
	uint16_t _port2;
	L4Protocol _l4Proto;

	NormalizedFlowIdentifier(
		const pcpp::IPAddress& srcIp,
		const pcpp::IPAddress& dstIp,
		uint16_t srcPort,
		uint16_t dstPort,
		L4Protocol l4Proto);

	/**
	 * @brief Comparison operators allowing use of this type in a std::set
	 */
	bool operator==(const NormalizedFlowIdentifier& other) const;
	bool operator!=(const NormalizedFlowIdentifier& other) const;
	bool operator<(const NormalizedFlowIdentifier& other) const;

	/**
	 * @brief Get a string representation of the identifier
	 */
	std::string ToString() const;
};

} // namespace generator
