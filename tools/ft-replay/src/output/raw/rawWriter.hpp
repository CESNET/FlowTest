/**
 * @file
 * @author Samuel Luptak <luptak@cesnet.cz>
 * @brief Raw writer interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "outputQueue.hpp"
#include "socketDescriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <linux/if_packet.h>
#include <memory>
#include <string>

namespace replay {

class RawWriter {
public:
	/**
	 * @brief Construct RawWriter
	 *
	 * @param[in] ifcName name of the output interface
	 */
	RawWriter(const std::string& ifcName);

	/**
	 * @brief Sends one packet
	 *
	 * @param[in] pkt reference to a packet to send
	 * @return The number of bytes sent
	 */
	int SendPacket(const PacketBuffer& pkt);

private:
	SocketDescriptor _socket;
	sockaddr_ll _sockAddr = {};

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("RawWriter");
};

} // namespace replay
