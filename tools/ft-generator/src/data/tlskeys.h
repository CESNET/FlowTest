/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Pregenerated TLS keys
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace generator {

struct TlsKeyData {
	std::string _cn;
	std::vector<uint8_t> _certDer;
	std::string _privKeyPem;
};

/**
 * These keys were generated using command:
 *    openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 3650 -nodes \
 *      -subj \
 *      "/C=XX/ST=StateName/L=CityName/O=CompanyName/OU=CompanySectionName/CN=CommonNameOrHostname"
 *
 * the certificates were transformed to DER format using command:
 *    openssl x509 -outform der <cert.pem >cert.der
 */
extern const std::vector<TlsKeyData> TLS_KEY_DATABASE;

} // namespace generator
