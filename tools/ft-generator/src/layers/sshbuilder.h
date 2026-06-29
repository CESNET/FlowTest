/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief SSH message builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../buffer.h"

namespace generator {

struct SshSettings;

class SshBuilder {
public:
	/**
	 * @brief Build the encrypted package data
	 *
	 * @param payloadSize The desired length of the resulting message
	 * @param set Current flows selected ssh settings (for MAC sizing)
	 *
	 * @return Buffer containing the ecrypted data
	 */
	Buffer BuildEncryptedPackage(std::size_t payloadSize, SshSettings& set);
};

} // namespace generator
