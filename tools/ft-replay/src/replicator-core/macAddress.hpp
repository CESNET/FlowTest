/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator Mac address
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace replay {

/**
 * @brief Represent basic operation over mac address
 */
class MacAddress {
public:
	using MacPtr = unsigned char*;

	MacAddress(MacPtr macPtr)
		: _isMacPtrAllocated(false)
		, _macPtr(macPtr)
	{
	}

	/**
	 * @brief Construct a new Mac Address from string representation
	 *
	 * @param macString MAC address as string
	 */
	MacAddress(const std::string& macString)
		: _isMacPtrAllocated(true)
	{
		_macPtr = new unsigned char[MAC_LENGTH];
		if (std::sscanf(
				macString.c_str(),
				"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
				&_macPtr[0],
				&_macPtr[1],
				&_macPtr[2],
				&_macPtr[3],
				&_macPtr[4],
				&_macPtr[5])
			!= 6) {
			throw std::runtime_error(macString + std::string(" is an invalid MAC address"));
		}
	}

	MacAddress(const MacAddress& macAddress)
	{
		if (macAddress._isMacPtrAllocated) {
			_macPtr = new unsigned char[MAC_LENGTH];
			std::memcpy(_macPtr, macAddress._macPtr, MAC_LENGTH);
		} else {
			_macPtr = macAddress._macPtr;
		}

		_isMacPtrAllocated = macAddress._isMacPtrAllocated;
	}

	MacAddress& operator=(const MacAddress& otherMac)
	{
		std::memcpy(_macPtr, otherMac._macPtr, MAC_LENGTH);
		return *this;
	}

	~MacAddress()
	{
		if (_isMacPtrAllocated) {
			delete[] _macPtr;

			_isMacPtrAllocated = false;
			_macPtr = nullptr;
		}
	}

private:
	static constexpr size_t MAC_LENGTH = 6;
	bool _isMacPtrAllocated;

	MacPtr _macPtr;
};

} // namespace replay
