/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Payload config section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tlsencryption.h"

#include <yaml-cpp/yaml.h>

#include <initializer_list>
#include <set>

namespace generator {
namespace config {

/**
 * @brief The possible payload protocols
 */
enum class PayloadProtocol { Http, Dns };

/**
 * @brief A list of payload protocols
 */
class PayloadProtocolList {
public:
	/**
	 * @brief Construct the list from specified values
	 *
	 * @param values The values
	 */
	PayloadProtocolList(std::initializer_list<PayloadProtocol> values);

	/**
	 * @brief Parse the protocol list from a yaml
	 *
	 * @param node The yaml node to parse
	 */
	PayloadProtocolList(const YAML::Node& node);

	/**
	 * @brief Check if the list includes a specific value
	 */
	bool Includes(PayloadProtocol value) const;

private:
	std::set<PayloadProtocol> _values;
};

/**
 * @brief Payload configuration section
 */
class Payload {
public:
	/**
	 * @brief Payload configuration section with default values
	 */
	Payload() = default;

	/**
	 * @brief Parse the payload configuration section node from a yaml
	 *
	 * @param node The yaml node to parse
	 */
	Payload(const YAML::Node& node);

	/**
	 * @brief Get the set of enabled protocols
	 */
	const PayloadProtocolList& GetEnabledProtocols() const { return _enabledProtocols; }

	/**
	 * @brief Get the TLS encryption configuration
	 */
	const TlsEncryption& GetTlsEncryption() const { return _tlsEncryption; }

private:
	PayloadProtocolList _enabledProtocols = {PayloadProtocol::Http, PayloadProtocol::Dns};
	TlsEncryption _tlsEncryption;
};

} // namespace config
} // namespace generator
