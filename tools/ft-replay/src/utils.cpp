/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary utilities
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils.hpp"
#include "socketDescriptor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <net/if.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>

namespace replay::utils {

bool CaseInsensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	const auto cmp = [](char lhsLetter, char rhsLetter) {
		return std::tolower(lhsLetter) == std::tolower(rhsLetter);
	};

	return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), cmp);
}

bool PowerOfTwo(uint64_t value)
{
	return value && (!(value & (value - 1)));
}

bool StrToBool(std::string_view str)
{
	for (const auto& item : {"yes", "true", "on", "1"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return true;
		}
	}

	for (const auto& item : {"no", "false", "off", "0"}) {
		if (CaseInsensitiveCompare(str, item)) {
			return false;
		}
	}

	throw std::invalid_argument("Unable to convert '" + std::string(str) + "' to boolean");
}

uint16_t GetInterfaceMTU(const std::string& name)
{
	SocketDescriptor socket;
	socket.OpenSocket(AF_INET, SOCK_DGRAM, 0);

	ifreq ifReq = {};
	memset(&ifReq, 0, sizeof(ifReq));
	snprintf(ifReq.ifr_name, IFNAMSIZ, "%s", name.c_str());
	if (ioctl(socket.GetSocketId(), SIOCGIFMTU, &ifReq) < 0) {
		throw std::runtime_error("Failed to get MTU of '" + std::string(name) + "' interface");
	}

	return ifReq.ifr_mtu;
}

NumaNode GetInterfaceNumaNode(const std::string& interface)
{
	std::string pathStart("/sys/class/net/");
	std::string pathEnd("/device/numa_node");
	std::string pathFinal = pathStart + interface + pathEnd;

	std::ifstream node(pathFinal);

	if (!node.is_open()) {
		return std::nullopt;
	}

	int ret;
	if (node >> ret && ret >= 0) {
		return ret;
	}

	return std::nullopt;
}

} // namespace replay::utils
