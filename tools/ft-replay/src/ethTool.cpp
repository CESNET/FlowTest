/**
 * @file
 * @brief EthTool class
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Ethtool C API
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ethTool.hpp"
#include "socketDescriptor.hpp"

#include <cstring>
#include <system_error>

#include <fcntl.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>

namespace replay {

template <typename T>
std::string BufferToStr(T& cstr)
{
	return {cstr, strnlen(cstr, sizeof(cstr))};
}

EthTool::EthTool(std::string_view ifcName)
	: _ifcName(ifcName)
{
	if (ifcName.length() > IFNAMSIZ - 1) {
		_logger->error("Interface name too long.");
		throw std::invalid_argument("EthTool::EthTool() has failed");
	}
}

int EthTool::ProcessRequest(void* ifrData)
{
	struct ifreq req {};

	SocketDescriptor sock;
	sock.OpenSocket(AF_INET, SOCK_DGRAM, 0);

	req.ifr_data = ifrData;
	snprintf(req.ifr_name, IFNAMSIZ, "%s", _ifcName.c_str());

	return ioctl(sock.GetSocketId(), SIOCETHTOOL, &req);
}

bool EthTool::GetDeviceState()
{
	struct ethtool_value info {};

	info.cmd = ETHTOOL_GLINK;

	int ret = ProcessRequest(&info);
	if (ret != 0) {
		_logger->error("Failed to get state from device");
		throw std::invalid_argument("EthTool::GetDeviceState() has failed");
	}

	return info.data;
}

struct EthTool::Channels EthTool::GetChannels()
{
	Channels result {};
	ethtool_channels info {};

	info.cmd = ETHTOOL_GCHANNELS;

	int ret = ProcessRequest(&info);
	if (ret != 0) {
		_logger->error("Failed to get channels count from device");
		throw std::invalid_argument("EthTool::GetChannelsCount() has failed");
	}

	result._txCount = info.tx_count;
	result._maxTxCount = info.max_tx;
	result._combinedCount = info.combined_count;
	result._maxCombinedCount = info.max_combined;

	return result;
}

struct EthTool::Driver EthTool::GetDriver()
{
	Driver result {};
	ethtool_drvinfo info {};

	info.cmd = ETHTOOL_GDRVINFO;

	int ret = ProcessRequest(&info);
	if (ret != 0) {
		_logger->error("Failed to get driver information");
		throw std::invalid_argument("EthTool::GetDriver() has failed");
	}

	result._driver = BufferToStr(info.driver);
	result._version = BufferToStr(info.version);
	result._fwVersion = BufferToStr(info.fw_version);
	result._busInfo = BufferToStr(info.bus_info);

	return result;
}

} // namespace replay
