/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Payload config section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "payload.h"
#include "common.h"

#include <map>

namespace generator {
namespace config {

static PayloadProtocol ParsePayloadProtocol(const YAML::Node& node)
{
	static std::map<std::string, PayloadProtocol> protocolNameToProtocol {
		{"http", PayloadProtocol::Http},
		{"dns", PayloadProtocol::Dns},
		{"tls", PayloadProtocol::Tls}};
	static std::string protocolNames = StringJoin(KeysOfMap(protocolNameToProtocol), ", ");

	std::string value = AsScalar(node);
	auto it = protocolNameToProtocol.find(value);
	if (it == protocolNameToProtocol.end()) {
		throw ConfigError(
			node,
			"invalid protocol name " + value + ", expected one of: " + protocolNames);
	}

	return it->second;
}

PayloadProtocolList::PayloadProtocolList(std::initializer_list<PayloadProtocol> values)
	: _values(values)
{
}

PayloadProtocolList::PayloadProtocolList(const YAML::Node& node)
{
	ExpectSequence(node);
	for (const auto& subnode : node) {
		ExpectScalar(subnode);
		auto value = ParsePayloadProtocol(subnode);
		if (Includes(value)) {
			throw ConfigError(node, "value " + AsScalar(subnode) + " specified multiple times");
		}
		_values.emplace(value);
	}
}

bool PayloadProtocolList::Includes(PayloadProtocol value) const
{
	return _values.find(value) != _values.end();
}

Payload::Payload(const YAML::Node& node)
{
	CheckAllowedKeys(node, {"enabled_protocols", "tls_encryption"});

	if (node["enabled_protocols"].IsDefined()) {
		_enabledProtocols = PayloadProtocolList(node["enabled_protocols"]);
	}

	if (node["tls_encryption"].IsDefined()) {
		_tlsEncryption = TlsEncryption(node["tls_encryption"]);
	}
}

} // namespace config
} // namespace generator
