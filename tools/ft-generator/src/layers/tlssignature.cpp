/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS signature class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tlssignature.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#include <cstring>
#include <stdexcept>

namespace generator {

TlsSignature::TlsSignature(const std::string& privateKeyPem)
{
	std::unique_ptr<BIO, decltype(&BIO_free)> bio(nullptr, &BIO_free);
	bio.reset(BIO_new_mem_buf(privateKeyPem.data(), privateKeyPem.size()));
	if (!bio) {
		throw std::runtime_error("BIO_new_mem_buf failed");
	}

	_pKey.reset(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
	if (!_pKey) {
		throw std::runtime_error("PEM_read_bio_PrivateKey failed");
	}

	_ctx.reset(EVP_MD_CTX_new());
	if (!_ctx) {
		throw std::runtime_error("EVP_MD_CTX_new failed");
	}

	int rc = EVP_DigestSignInit(_ctx.get(), nullptr, EVP_sha256(), nullptr, _pKey.get());
	if (rc != 1) {
		throw std::runtime_error("EVP_DigestSignInit failed");
	}
}

void TlsSignature::Digest(const uint8_t* data, std::size_t length)
{
	int rc = EVP_DigestSignUpdate(_ctx.get(), data, length);
	if (rc != 1) {
		throw std::runtime_error("EVP_DigestUpdate failed");
	}
}

std::vector<uint8_t> TlsSignature::Finalize()
{
	std::size_t len = 0;
	int rc = EVP_DigestSignFinal(_ctx.get(), nullptr, &len);
	if (rc != 1) {
		throw std::runtime_error("EVP_DigestFinal failed");
	}

	std::vector<uint8_t> sig(len);
	len = sig.size();
	rc = EVP_DigestSignFinal(_ctx.get(), sig.data(), &len);
	if (rc != 1) {
		throw std::runtime_error("EVP_DigestFinal failed");
	}
	sig.resize(len);

	return sig;
}

} // namespace generator
