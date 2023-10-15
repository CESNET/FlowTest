/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TlsEncryption configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tlsencryption.h"
#include "common.h"

namespace generator {
namespace config {

TlsEncryption::TlsEncryption(const YAML::Node& node)
{
	CheckAllowedKeys(
		node,
		{"always_encrypt_ports", "never_encrypt_ports", "otherwise_with_probability"});

	if (node["always_encrypt_ports"].IsDefined()) {
		_alwaysEncryptPorts = ParseMany<PortRange>(node["always_encrypt_ports"]);
	}

	if (node["never_encrypt_ports"].IsDefined()) {
		_neverEncryptPorts = ParseMany<PortRange>(node["never_encrypt_ports"]);
	}

	if (node["otherwise_with_probability"].IsDefined()) {
		_otherwiseEncryptProbability = ParseProbability(node["otherwise_with_probability"]);
	}
}

} // namespace config
} // namespace generator
