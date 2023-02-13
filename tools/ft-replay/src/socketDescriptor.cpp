/**
 * @file
 * @brief  Implementation of SocketDescriptor class.
 * @author Matej Hulak <hulak@cesnet.cz>
 * @date   June 2022
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "socketDescriptor.hpp"

#include <net/if.h>
#include <unistd.h>

namespace replay {

SocketDescriptor::SocketDescriptor() {}

SocketDescriptor::SocketDescriptor(SocketDescriptor&& other) noexcept
	: _socketId(other._socketId)
{
	other._socketId = -1;
}

SocketDescriptor& SocketDescriptor::operator=(SocketDescriptor&& other) noexcept
{
	if (this == &other) {
		return *this;
	}
	_socketId = other._socketId;
	other._socketId = -1;
	return *this;
}

SocketDescriptor::~SocketDescriptor()
{
	if (_socketId != -1 && close(_socketId) != 0) {
		_logger->error("Error while closing socket!");
	}
}

void SocketDescriptor::OpenSocket(const int family, const int type, const int procotol)
{
	_socketId = socket(family, type, procotol);
	if (_socketId == -1) {
		_logger->error("Cannot open socket (maybe insufficient privilege)");
		throw std::runtime_error("SocketDescriptor::OpenSocket() has failed");
	}
}

int SocketDescriptor::GetSocketId() const noexcept
{
	return _socketId;
}

} // namespace replay
