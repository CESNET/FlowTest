/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS signature class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>
#include <string>
#include <vector>

namespace generator {

/**
 * @brief Class for computation of TLS signature
 */
class TlsSignature {
public:
	/**
	 * @brief Construct a TLS signature instance
	 *
	 * @param privateKeyPem The private key in PEM format
	 */
	TlsSignature(const std::string& privateKeyPem);

	/**
	 * @brief Digest some data
	 *
	 * @param data The bytes to digest
	 * @param length The number of bytes
	 */
	void Digest(const uint8_t* data, std::size_t length);

	/**
	 * @brief Finalize the signature
	 *
	 * @return The signature bytes
	 */
	std::vector<uint8_t> Finalize();

private:
	// WARNING: ctx depends on pKey, therefore the members should remain in this order to ensure
	//          that they are freed in the correct order!
	std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> _pKey {nullptr, &EVP_PKEY_free};
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> _ctx {nullptr, &EVP_MD_CTX_free};
};

} // namespace generator
