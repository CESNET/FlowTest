/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Port range
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "portrange.h"
#include "common.h"

namespace generator {
namespace config {

PortRange::PortRange(const YAML::Node& node)
{
	const std::string& str = AsScalar(node);
	const auto& parts = StringSplit(str, "-");
	const auto& err = ConfigError(node, "expected port or port range (e.g. 1000 or 0-1000)");

	if (parts.size() == 2) {
		auto maybeValue1 = ParseValue<uint16_t>(StringStrip(parts[0]));
		auto maybeValue2 = ParseValue<uint16_t>(StringStrip(parts[1]));
		if (!maybeValue1 || !maybeValue2) {
			throw err;
		}
		_from = *maybeValue1;
		_to = *maybeValue2;
		if (_from > _to) {
			throw ConfigError(node, "invalid port range value, left side > right side");
		}

	} else if (parts.size() == 1) {
		auto maybeValue = ParseValue<uint16_t>(StringStrip(parts[0]));
		if (!maybeValue) {
			throw err;
		}
		_from = *maybeValue;
		_to = *maybeValue;

	} else {
		throw err;
	}
}

PortRange::PortRange(uint16_t from, uint16_t to)
	: _from(from)
	, _to(to)
{
	if (_from > _to) {
		throw std::logic_error("invalid port range value, left side > right side");
	}
}

PortRange::PortRange(uint16_t value)
	: _from(value)
	, _to(value)
{
}

bool PortRange::Includes(uint16_t value) const
{
	return _from <= value && value <= _to;
}

} // namespace config
} // namespace generator
