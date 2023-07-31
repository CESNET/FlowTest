/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Mac address generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "macaddressgenerator.h"

#include "logger.h"

namespace generator {

MacAddressGenerator::MacAddressGenerator(MacAddress baseAddr, uint8_t prefixLen)
	: _gen(PrefixedGenerator(
		std::vector<uint8_t>(baseAddr.getRawData(), baseAddr.getRawData() + 6),
		prefixLen))
	, _prefixLen(prefixLen)
{
	if (prefixLen > 0 && !baseAddr.isValid()) {
		throw std::invalid_argument("invalid MacAddressGenerator base address");
	}

	if (prefixLen >= 8 && baseAddr.getRawData()[0] & 1) {
		uint8_t newMacData[6];
		baseAddr.copyTo(newMacData);
		newMacData[0] &= ~uint8_t(1);
		MacAddress newMac(newMacData);

		auto logger = ft::LoggerGet("MacAddressGenerator");
		logger->warn(
			"MAC address prefix {} will lead to invalid mac addresses being generated. LSB of the "
			"first octet must be 0 for non-group addresses (802.3-2002 3.2.3b). Consider {} "
			"instead.",
			baseAddr.toString(),
			newMac.toString());
	}
}

MacAddress MacAddressGenerator::Generate()
{
	MacAddress mac;

	while (true) {
		mac = MacAddress(_gen.Generate().data());

		/**
		 * LSB=0 in the first octet denotes a group address.
		 *
		 * 802.3-2002 3.2.3(b):
		 * b) The first bit (LSB) shall be used in the Destination Address field as an address type
		 * designation bit to identify the Destination Address either as an individual or as a group
		 * address. If this bit is 0, it shall indicate that the address field contains an
		 * individual address. If this bit is 1, it shall indicate that the address field contains a
		 * group address that identifies none, one or more, or all of the stations connected to the
		 * LAN. In the Source Address field, the first bit is reserved and set to 0.
		 */
		if (_prefixLen <= 7 && mac.getRawData()[0] & 1) {
			continue;
		}

		break;
	}

	return mac;
}

} // namespace generator
