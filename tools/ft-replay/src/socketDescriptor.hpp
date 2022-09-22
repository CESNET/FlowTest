/**
 * @file
 * @brief  SocketDescriptor class.
 * @author Matej Hulak <hulak@cesnet.cz>
 * @date   June 2022
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <iostream>

namespace replay {

class SocketDescriptor {
public:
	/**
	 * @brief Construct SocketDescriptor
	 */
	SocketDescriptor();
	/**
	 * @brief Move constructor
	 *
	 * @param[in] SocketDescriptor r-value
	 */
	SocketDescriptor(SocketDescriptor&& other);
	/**
	 * @brief Move assignment
	 *
	 * @param[in] SocketDescriptor r-value
	 *
	 * @return reference to current object
	 */
	SocketDescriptor& operator=(SocketDescriptor&& other);
	/**
	 * @brief Destructor, closes socket
	 */
	~SocketDescriptor();

	SocketDescriptor(const SocketDescriptor&) = delete;
	SocketDescriptor& operator=(const SocketDescriptor&) = delete;

	/**
	 * @brief Open socket
	 *
	 * Opens socket and gets ifc ID
	 *
	 * @param[in] socket family
	 * @param[in] socket type
	 * @param[in] protocol family
	 *
	 * @throw runtime_error socket cannot be opened
	 */
	void OpenSocket(const int family, const int type, const int procotol);
	/**
	 * @brief Get socket ID
	 *
	 * Returns socket ID
	 *
	 * @return socketID
	 */
	int GetSocketId() const noexcept;

private:
	int _socketId = -1;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("SocketDescriptor");
};

} // namespace replay
