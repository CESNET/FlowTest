/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS message builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../buffer.h"
#include "../data/tlskeys.h"

namespace generator {

/**
 * @brief Specialized buffer class for TLS messages
 */
class TlsBuffer : public Buffer {
public:
	/**
	 * @brief Append a length field and remember its position in a stack
	 */
	template <typename T>
	void PushLength();

	/**
	 * @brief Pop a length field from the stack and update its value based on the current
	 *        position
	 */
	template <typename T>
	void PopLength();

private:
	std::vector<std::size_t> _lengthOffsetStack;
};

template <typename T>
void TlsBuffer::PushLength()
{
	_lengthOffsetStack.push_back(Length());
	Append<T>(0);
}

template <typename T>
void TlsBuffer::PopLength()
{
	assert(!_lengthOffsetStack.empty());
	auto offset = _lengthOffsetStack.back();
	_lengthOffsetStack.pop_back();
	WriteAt<T>(offset, Length() - offset - sizeof(T));
}

/**
 * @brief A TLS message builder class
 */
class TlsBuilder {
public:
	TlsBuilder();

	/**
	 * @brief Build the ClientHello message
	 *
	 * @return Buffer containing the ClientHello message
	 */
	Buffer BuildClientHello();

	/**
	 * @brief Build the ServerHello message
	 *
	 * @return Buffer containing the ServerHello message
	 */
	Buffer BuildServerHello();

	/**
	 * @brief Build the Certificate message
	 *
	 * @return Buffer containing the Certificate message
	 */
	Buffer BuildCertificate();

	/**
	 * @brief Build the ServerKeyExchange message
	 *
	 * @return Buffer containing the ServerKeyExchange message
	 */
	Buffer BuildServerKeyExchange();

	/**
	 * @brief Build the ServerHelloDone message
	 *
	 * @return Buffer containing the ServerHelloDone message
	 */
	Buffer BuildServerHelloDone();

	/**
	 * @brief Build the ClientKeyExchange message
	 *
	 * @return Buffer containing the ClientKeyExchange message
	 */
	Buffer BuildClientKeyExchange();

	/**
	 * @brief Build the ChangeCipherSpec message
	 *
	 * @return Buffer containing the ChangeCipherSpec message
	 */
	Buffer BuildChangeCipherSpec();

	/**
	 * @brief Build the EncryptedHandshake message
	 *
	 * @return Buffer containing the EncryptedHandshake message
	 */
	Buffer BuildEncryptedHandshake();

	/**
	 * @brief Build the ApplicationData message
	 *
	 * @param recordLength The desired length of the resulting message
	 *
	 * @return Buffer containing the ApplicationData message
	 */
	Buffer BuildApplicationData(std::size_t recordLength);

private:
	std::vector<uint8_t> _clientRandom;
	std::vector<uint8_t> _serverRandom;
	const TlsKeyData* _keyData;
};

} // namespace generator
