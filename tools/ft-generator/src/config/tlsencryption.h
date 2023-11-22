/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TlsEncryption configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "portrange.h"

#include <cstdint>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief A representation of the TlsEncryption section in the yaml config
 */
class TlsEncryption {
public:
	/**
	 * @brief Construct a new TlsEncryption configuration object
	 */
	TlsEncryption() = default;

	/**
	 * @brief Construct a new TlsEncryption configuration object from a yaml node
	 */
	TlsEncryption(const YAML::Node& node);

	/**
	 * @brief Get list of ports to always encrypt
	 */
	const std::vector<PortRange>& GetAlwaysEncryptPorts() const { return _alwaysEncryptPorts; }

	/**
	 * @brief Get list of ports to never encrypt
	 */
	const std::vector<PortRange>& GetNeverEncryptPorts() const { return _neverEncryptPorts; }

	/**
	 * @brief Get encryption probability otherwise
	 */
	double GetOtherwiseEncryptProbability() const { return _otherwiseEncryptProbability; }

private:
	std::vector<PortRange> _alwaysEncryptPorts = {{443}};
	std::vector<PortRange> _neverEncryptPorts = {{80}};
	double _otherwiseEncryptProbability = 0.3;
};

} // namespace config
} // namespace generator
