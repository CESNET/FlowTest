/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS message builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tlsbuilder.h"
#include "../randomgenerator.h"
#include "tlsconstants.h"
#include "tlssignature.h"

#include <algorithm>

namespace generator {

static std::vector<uint16_t> RandomCiphersuitesExcept(uint16_t exceptCiphersuite)
{
	std::vector<uint16_t> values = TLS_CIPHERSUITES_LIST;
	values.erase(std::remove(values.begin(), values.end(), exceptCiphersuite), values.end());

	auto& rng = RandomGenerator::GetInstance();
	rng.Shuffle(values);
	values.resize(rng.RandomUInt(0, values.size()));

	return values;
}

TlsBuilder::TlsBuilder()
{
	auto& rng = RandomGenerator::GetInstance();

	_keyData = &rng.RandomChoice(TLS_KEY_DATABASE);
	_clientRandom = rng.RandomBytes(TLS_CLIENT_RANDOM_LENGTH);
	_serverRandom = rng.RandomBytes(TLS_SERVER_RANDOM_LENGTH);
}

Buffer TlsBuilder::BuildClientHello()
{
	auto& rng = RandomGenerator::GetInstance();

	TlsBuffer buf;

	// Content-Type
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	// Version
	buf.Append<UInt16Be>(TLS_VERSION_1_0);
	// Length
	buf.PushLength<UInt16Be>();

	// Handshake protocol
	//// Handshake type
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_CLIENT_HELLO);
	//// Length
	buf.PushLength<UInt24Be>();
	//// Actual version
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Random
	buf.Append(_clientRandom);
	//// Session ID length
	buf.Append<uint8_t>(TLS_SESSION_ID_LENGTH);
	//// Session ID
	buf.Append(rng.RandomBytes(TLS_SESSION_ID_LENGTH));
	//// Cipher Suites Length
	buf.PushLength<UInt16Be>();
	//// Cipher suites
	buf.Append<UInt16Be>(TLS_CIPHERSUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
	for (auto ciphersuite :
		 RandomCiphersuitesExcept(TLS_CIPHERSUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384)) {
		buf.Append<UInt16Be>(ciphersuite);
	}
	buf.Append<UInt16Be>(TLS_CIPHERSUITE_EMPTY_RENEGOTIATION_INFO_SCSV);
	//
	buf.PopLength<UInt16Be>();
	//// Compression methods length
	buf.PushLength<uint8_t>();
	//// Compression methods
	buf.Append<uint8_t>(TLS_COMPRESSION_METHOD_NONE);
	//
	buf.PopLength<uint8_t>();

	//// Extensions
	//// Extensions length
	buf.PushLength<UInt16Be>();

	//// Server Name
	const std::string& serverName = _keyData->_cn;
	////// Extension type
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_SERVER_NAME);
	////// Extension length
	buf.PushLength<UInt16Be>();
	////// Server name list length
	buf.PushLength<UInt16Be>();
	////// Server name type
	buf.Append<uint8_t>(TLS_SERVER_NAME_TYPE_HOSTNAME);
	////// Server name length
	buf.Append<UInt16Be>(serverName.size());
	////// Server name
	buf.Append(serverName);
	//
	buf.PopLength<UInt16Be>();
	buf.PopLength<UInt16Be>();

	// Extension: ec_point_formats (len=4)
	// Type: ec_point_formats (11)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_EC_POINT_FORMATS);
	// Length: 4
	buf.PushLength<UInt16Be>();
	// EC point formats Length: 3
	buf.PushLength<uint8_t>();
	// Elliptic curves point formats (3)
	// EC point format: uncompressed (0)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_UNCOMPRESSED);
	// EC point format: ansiX962_compressed_prime (1)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_ANSIX962_COMPRESSED_PRIME);
	// EC point format: ansiX962_compressed_char2 (2)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_ANSIX962_COMPRESSED_CHAR2);
	//
	buf.PopLength<uint8_t>();
	buf.PopLength<UInt16Be>();

	// Extension: supported_groups (len=10)
	// Type: supported_groups (10)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_SUPPORTED_GROUPS);
	// Length: 10
	buf.PushLength<UInt16Be>();
	// Supported Groups List Length: 8
	buf.PushLength<UInt16Be>();
	// Supported Groups (4 groups)
	// Supported Group: secp256r1 (0x0017)
	buf.Append<UInt16Be>(TLS_SUPPORTED_GROUP_SECP256R1);
	// Supported Group: secp521r1 (0x0019)
	buf.Append<UInt16Be>(TLS_SUPPORTED_GROUP_SECP521R1);
	// Supported Group: secp384r1 (0x0018)
	buf.Append<UInt16Be>(TLS_SUPPORTED_GROUP_SECP384R1);
	// Supported Group: secp256k1 (0x0016)
	buf.Append<UInt16Be>(TLS_SUPPORTED_GROUP_SECP256K1);
	//
	buf.PopLength<UInt16Be>();
	buf.PopLength<UInt16Be>();

	// Extension: signature_algorithms
	// Type: signature_algorithms (13)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_SIGNATURE_ALGORITHMS);
	// Length
	buf.PushLength<UInt16Be>();
	// Signature Hash Algorithms Length
	buf.PushLength<UInt16Be>();
	// Signature Hash Algorithms
	// Signature Algorithm: rsa_pkcs1_sha512 (0x0601)
	buf.Append<UInt16Be>(TLS_SIGNATURE_ALGORITHM_RSA_PKCS1_SHA512);
	// Signature Algorithm: SHA512 DSA (0x0602)
	buf.Append<UInt16Be>(TLS_SIGNATURE_ALGORITHM_SHA512_DSA);
	//
	buf.PopLength<UInt16Be>();
	buf.PopLength<UInt16Be>();

	// Extension: application_layer_protocol_negotiation
	// Type: application_layer_protocol_negotiation (16)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_APPLICATION_LAYER_PROTOCOL_NEGOTIATION);
	///// Length: 14
	buf.PushLength<UInt16Be>();
	///// ALPN Extension Length: 12
	buf.PushLength<UInt16Be>();
	///// ALPN Protocol
	/////// ALPN string length: 2
	/////// ALPN Next Protocol: h2
	buf.PushLength<uint8_t>();
	buf.Append("h2");
	buf.PopLength<uint8_t>();
	/////// ALPN string length: 8
	/////// ALPN Next Protocol: http/1.1
	buf.PushLength<uint8_t>();
	buf.Append("http/1.1");
	buf.PopLength<uint8_t>();
	//
	buf.PopLength<UInt16Be>();
	buf.PopLength<UInt16Be>();

	// Extensions length
	buf.PopLength<UInt16Be>();
	// Handshake length
	buf.PopLength<UInt24Be>();
	// TLS record length
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildServerHello()
{
	TlsBuffer buf;

	// Content-Type
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	// Version
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	// Length
	buf.PushLength<UInt16Be>();

	// Handshake protocol
	//// Handshake type
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_SERVER_HELLO);
	//// Length
	buf.PushLength<UInt24Be>();
	//// Actual version
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Random
	buf.Append(_serverRandom);
	//// Session ID length
	buf.Append<uint8_t>(0);
	//// Session ID
	// none
	//// Cipher suite
	buf.Append<UInt16Be>(TLS_CIPHERSUITE_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
	//// Compression method
	buf.Append<uint8_t>(TLS_COMPRESSION_METHOD_NONE);

	//// Extensions
	//// Extensions length
	buf.PushLength<UInt16Be>();

	// Extension: ec_point_formats (len=4)
	// Type: ec_point_formats (11)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_EC_POINT_FORMATS);
	// Length: 4
	buf.PushLength<UInt16Be>();
	// EC point formats Length: 3
	buf.PushLength<uint8_t>();
	// Elliptic curves point formats (3)
	// EC point format: uncompressed (0)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_UNCOMPRESSED);
	// EC point format: ansiX962_compressed_prime (1)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_ANSIX962_COMPRESSED_PRIME);
	// EC point format: ansiX962_compressed_char2 (2)
	buf.Append<uint8_t>(TLS_EC_POINT_FORMAT_ANSIX962_COMPRESSED_CHAR2);
	//
	buf.PopLength<uint8_t>();
	buf.PopLength<UInt16Be>();

	// Extension: application_layer_protocol_negotiation
	// Type: application_layer_protocol_negotiation (16)
	buf.Append<UInt16Be>(TLS_EXTENSION_TYPE_APPLICATION_LAYER_PROTOCOL_NEGOTIATION);
	///// Length: 14
	buf.PushLength<UInt16Be>();
	///// ALPN Extension Length: 12
	buf.PushLength<UInt16Be>();
	///// ALPN Protocol
	/////// ALPN string length: 2
	/////// ALPN Next Protocol: h2
	buf.PushLength<uint8_t>();
	buf.Append("h2");
	buf.PopLength<uint8_t>();
	//
	buf.PopLength<UInt16Be>();
	buf.PopLength<UInt16Be>();

	// Extensions length
	buf.PopLength<UInt16Be>();
	// Handshake length
	buf.PopLength<UInt24Be>();
	// Total length
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildCertificate()
{
	TlsBuffer buf;

	// TLSv1.2 Record Layer: Handshake Protocol: Certificate
	//// Content Type: Handshake (22)
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	//// Version: TLS 1.2 (0x0303)
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Length
	buf.PushLength<UInt16Be>();
	//// Handshake Protocol: Certificate
	////// Handshake Type: Certificate (11)
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_CERTIFICATE);
	////// Length
	buf.PushLength<UInt24Be>();
	////// Certificates Length
	buf.PushLength<UInt24Be>();
	////// Certificates
	//////// Certificate Length
	buf.PushLength<UInt24Be>();
	//////// Certificate
	buf.Append((const char*) _keyData->_certDer.data(), _keyData->_certDer.size());
	//////// Certificate Length
	//////// Certificate:
	// <Optionally more certificates for the whole certificate chain>
	//
	buf.PopLength<UInt24Be>(); // Certificate length
	buf.PopLength<UInt24Be>(); // Certificates length
	buf.PopLength<UInt24Be>(); // Handshake length
	buf.PopLength<UInt16Be>(); // Total length

	return buf;
}

Buffer TlsBuilder::BuildServerKeyExchange()
{
	auto& rng = RandomGenerator::GetInstance();
	std::vector<uint8_t> pubKey = rng.RandomBytes(65);

	TlsSignature sig(_keyData->_privKeyPem);

	Buffer curveInfoBuf;
	curveInfoBuf.Append<uint8_t>(TLS_CURVE_TYPE_NAMED_CURVE);
	curveInfoBuf.Append<UInt16Be>(TLS_NAMED_CURVE_SECP256R1);

	// Signature is calculated as:
	//   sign(hash(clientRandom + serverRandom + curveInfo + pubKey))
	sig.Digest(_clientRandom.data(), _clientRandom.size());
	sig.Digest(_serverRandom.data(), _serverRandom.size());
	sig.Digest(reinterpret_cast<const uint8_t*>(curveInfoBuf.PtrAt(0)), curveInfoBuf.Length());
	sig.Digest(pubKey.data(), pubKey.size());

	const std::vector<uint8_t>& signature = sig.Finalize();

	TlsBuffer buf;

	// TLSv1.2 Record Layer: Handshake Protocol: Server Key Exchange
	//// Content Type: Handshake (22)
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	//// Version: TLS 1.2 (0x0303)
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Length
	buf.PushLength<UInt16Be>();
	//// Handshake Protocol: Server Key Exchange
	////// Handshake Type: Server Key Exchange (12)
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_SERVER_KEY_EXCHANGE);
	////// Length
	buf.PushLength<UInt24Be>();
	////// EC Diffie-Hellman Server Params
	//////// Curve Type: named_curve (0x03)
	buf.Append<uint8_t>(TLS_CURVE_TYPE_NAMED_CURVE);
	//////// Named Curve: secp256r1 (0x0017)
	buf.Append<UInt16Be>(TLS_NAMED_CURVE_SECP256R1);
	//////// Pubkey Length
	buf.Append<uint8_t>(pubKey.size());
	//////// Pubkey
	buf.Append(pubKey);
	//////// Signature Algorithm: rsa_pkcs1_sha512 (0x0601)
	////////// Signature Hash Algorithm Hash: SHA512 (6)
	////////// Signature Hash Algorithm Signature: RSA (1)
	buf.Append<UInt16Be>(TLS_SIGNATURE_ALGORITHM_RSA_PKCS1_SHA512);
	//////// Signature Length
	buf.Append<UInt16Be>(signature.size());
	//////// Signature
	buf.Append(signature);
	//
	buf.PopLength<UInt24Be>();
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildServerHelloDone()
{
	// Server Hello done
	TlsBuffer buf;
	//// Content Type: Handshake (22)
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	//// Version: TLS 1.2 (0x0303)
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Length
	buf.PushLength<UInt16Be>();
	// Handshake type
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_SERVER_HELLO_DONE);
	// Handshake length
	buf.Append<UInt24Be>(0);
	//
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildClientKeyExchange()
{
	auto& rng = RandomGenerator::GetInstance();

	// Client key exchange
	TlsBuffer buf;
	//// Content Type: Handshake (22)
	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE);
	//// Version: TLS 1.2 (0x0303)
	buf.Append<UInt16Be>(TLS_VERSION_1_2);
	//// Length
	buf.PushLength<UInt16Be>();
	// Handshake type
	buf.Append<uint8_t>(TLS_HANDSHAKE_TYPE_CLIENT_KEY_EXCHANGE);
	// Handshake length
	buf.PushLength<UInt24Be>();
	// Pubkey length
	buf.PushLength<uint8_t>();
	// Pubkey
	buf.Append(rng.RandomBytes(65));
	//
	buf.PopLength<uint8_t>();
	buf.PopLength<UInt24Be>();
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildChangeCipherSpec()
{
	TlsBuffer buf;

	buf.Append<uint8_t>(TLS_CONTENT_TYPE_CHANGE_CIPHER_SPEC); // Content Type
	buf.Append<UInt16Be>(TLS_VERSION_1_2); // Version
	buf.PushLength<UInt16Be>(); // Length
	buf.Append<uint8_t>(1); // Change cipher spec
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildEncryptedHandshake()
{
	auto& rng = RandomGenerator::GetInstance();

	TlsBuffer buf;

	buf.Append<uint8_t>(TLS_CONTENT_TYPE_HANDSHAKE); // Content Type
	buf.Append<UInt16Be>(TLS_VERSION_1_2); // Version
	buf.PushLength<UInt16Be>(); // Length
	buf.Append(rng.RandomBytes(40)); // "Encrypted data"
	buf.PopLength<UInt16Be>();

	return buf;
}

Buffer TlsBuilder::BuildApplicationData(std::size_t recordLength)
{
	auto& rng = RandomGenerator::GetInstance();
	TlsBuffer buf;

	buf.Append<uint8_t>(TLS_CONTENT_TYPE_APPLICATION_DATA); // Content Type
	buf.Append<UInt16Be>(TLS_VERSION_1_2); // Version
	buf.PushLength<UInt16Be>(); // Length

	if (buf.Length() > recordLength) {
		throw std::logic_error("recordLength must be at least " + std::to_string(buf.Length()));
	}

	buf.Append(rng.RandomBytes(recordLength - buf.Length())); // "Encrypted data"
	buf.PopLength<UInt16Be>();

	return buf;
}

} // namespace generator
