/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief Database of available SSH algorithms and keys
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// The sshdata.cpp file is generated using script generate_ssh_data.py

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace generator {

// KEX algorithms structure
struct SshKexAlgorithms {
	std::vector<std::pair<std::string, uint16_t>> must;
	std::vector<std::pair<std::string, uint16_t>> should;
	std::vector<std::pair<std::string, uint16_t>> may;
	std::vector<std::pair<std::string, uint16_t>> available;
};

// Algorithms
// for databases with std::pair the size of the algorithm is in bits
// Source: https://www.iana.org/assignments/ssh-parameters/ssh-parameters.xhtml#ssh-parameters-16
extern const SshKexAlgorithms SSH_KEX_ALGORITHM_DATABASE;
// Source: https://www.iana.org/assignments/ssh-parameters/ssh-parameters.xhtml#ssh-parameters-17
extern const std::vector<std::string> SSH_ENCRYPTION_ALGORITHM_DATABASE;
// Source: https://www.iana.org/assignments/ssh-parameters/ssh-parameters.xhtml#ssh-parameters-18
extern const std::vector<std::pair<std::string, uint16_t>> SSH_MAC_ALGORITHM_DATABASE;
// Source: https://www.iana.org/assignments/ssh-parameters/ssh-parameters.xhtml#ssh-parameters-20
extern const std::vector<std::string> SSH_COMPRESSION_ALGORITHM_DATABASE;
// Source: https://www.iana.org/assignments/ssh-parameters/ssh-parameters.xhtml#ssh-parameters-19
extern const std::vector<std::pair<std::string, uint16_t>> SSH_PUB_KEY_ALGORITHM_DATABASE;
// The strings are selected and formated fromthis source:
// https://github.com/nmap/nmap/blob/master/nmap-service-probes
extern const std::vector<std::string> SSH_SOFTWARE_STRINGS;

// Keys
// the key sizes are in bytes
// All keys and signatures are generated using the cryptography python library:
// https://pypi.org/project/cryptography/
extern const std::vector<std::array<uint8_t, 32>> SSH_KEX_CURVE25519_SHA256_KEYS;
extern const std::vector<std::array<uint8_t, 256>> SSH_KEX_DIFFIE_HELLMAN_GROUP14_SHA256_KEYS;
extern const std::vector<std::array<uint8_t, 65>> SSH_KEX_ECDH_SHA2_NISTP256_KEYS;
extern const std::vector<std::array<uint8_t, 32>> SSH_ED25519_KEYS;
extern const std::vector<std::array<uint8_t, 64>> SSH_ED25519_SIGNATURES;
extern const std::vector<std::array<uint8_t, 256>> SSH_RSA_2048_KEYS;
extern const std::vector<std::array<uint8_t, 256>> SSH_RSA_2048_SIGNATURES;
extern const std::vector<std::array<uint8_t, 128>> SSH_DSA_1024_KEYS;
extern const std::vector<std::array<uint8_t, 48>> SSH_DSA_1024_SIGNATURES;

} // namespace generator
