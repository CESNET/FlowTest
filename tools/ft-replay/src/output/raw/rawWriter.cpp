/**
 * @file
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw writer implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rawWriter.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <linux/if_packet.h>
#include <memory>
#include <net/if.h>
#include <sys/ioctl.h>

namespace replay {

RawWriter::RawWriter(const std::string& ifcName)
{
	_socket.OpenSocket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);

	ifreq ifReq = {};
	snprintf(ifReq.ifr_name, IFNAMSIZ, "%s", ifcName.c_str());

	if ((ioctl(_socket.GetSocketId(), SIOCGIFINDEX, &ifReq)) < 0) {
		_logger->error("Cannot obtain interface id");
		throw std::runtime_error("RawWriter::RawWriter() has failed");
	}

	_sockAddr.sll_ifindex = ifReq.ifr_ifindex;
}

int RawWriter::SendPacket(const PacketBuffer& pkt)
{
	return sendto(
		_socket.GetSocketId(),
		pkt._data,
		pkt._len,
		0,
		(const sockaddr*) &_sockAddr,
		sizeof(sockaddr_ll));
}

} // namespace replay
