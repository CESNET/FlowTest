/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief SSH message builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../buffer.h"
#include "../packet.h"

namespace generator {

struct SshSettings;

class SshBuilder {
public:
	/**
	 * @brief Build the ID string packet message
	 *
	 * @return Buffer with the ID
	 */
	Buffer BuildIdString();

	/**
	 * @brief Build the key exchange message
	 *
	 * @param set The SshSettings structure
	 * @param dir The direction of the packet
	 *
	 * @return Buffer with the message
	 */
	Buffer BuildKexInit(SshSettings& set, const Direction& dir);

	/**
	 * @brief Build the client ECDH (and other algoritm) key exchange
	 *
	 * @param set The SshSettings structure
	 *
	 * @return Buffer with the client ECDH key exchange
	 */
	Buffer BuildClientECDH(SshSettings& set);

	/**
	 * @brief Build the server ECDH (and other algoritm) key exchange
	 *
	 * @param set The SshSettings structure
	 *
	 * @return Buffer with the serverECDH key exchange
	 */
	Buffer BuildServerECDH(SshSettings& set);

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
