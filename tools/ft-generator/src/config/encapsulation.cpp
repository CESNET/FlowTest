/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Encapsulation configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "encapsulation.h"

namespace generator {
namespace config {

constexpr uint32_t MPLS_LABEL_MAX = 1048575; // 20 bits, 2^20 - 1
constexpr uint16_t VLAN_ID_MIN = 1;
constexpr uint16_t VLAN_ID_MAX = 4095;

EncapsulationLayerVlan::EncapsulationLayerVlan(const YAML::Node& node)
{
	checkAllowedKeys(node, {
		"type",
		"id"
	});
	auto id = parseValue<uint16_t>(asScalar(node["id"]));
	if (!id || *id < VLAN_ID_MIN || *id > VLAN_ID_MAX) {
		throw ConfigError(node, "invalid vlan id value, expected integer in range <1, 4095>");
	}
	_id = *id;
}

EncapsulationLayerMpls::EncapsulationLayerMpls(const YAML::Node& node)
{
	checkAllowedKeys(node, {
		"type",
		"label"
	});
	auto label = parseValue<uint32_t>(asScalar(node["label"]));
	if (!label || *label > MPLS_LABEL_MAX) {
		throw ConfigError(node, "invalid mpls label value, expected integer in range <0, 1048575>");
	}
	_label = *label;
}


EncapsulationLayer::EncapsulationLayer(const YAML::Node& node)
{
	expectKey(node, "type");
	const std::string& type = asScalar(node["type"]);
	if (type == "vlan") {
		*this = EncapsulationLayerVlan(node);
	} else if (type == "mpls") {
		*this = EncapsulationLayerMpls(node);
	} else {
		throw ConfigError(node, "invalid encapsulation layer type \"" + type + "\", "
			"allowed types are \"vlan\", \"mpls\"");
	}
}

EncapsulationVariant::EncapsulationVariant(const YAML::Node& node)
{
	checkAllowedKeys(node, {
		"probability",
		"layers"
	});
	expectKey(node, "probability");
	expectKey(node, "layers");

	_probability = parseProbability(node["probability"]);
	_layers = parseMany<EncapsulationLayer>(node["layers"]);
	for (size_t i = 1; i < _layers.size(); i++) {
		if (std::holds_alternative<EncapsulationLayerMpls>(_layers[i - 1])
				&& std::holds_alternative<EncapsulationLayerVlan>(_layers[i])) {
			throw ConfigError(node["layers"][i],
				"invalid combination of encapsulation layers, \"vlan\" cannot follow \"mpls\"");
		}
	}
}

Encapsulation::Encapsulation(const YAML::Node& node) :
	_variants(parseOneOrMany<EncapsulationVariant>(node))
{
	double sum = 0.0;
	for (size_t i = 0; i < _variants.size(); i++) {
		sum += _variants[i].GetProbability();
		if (sum > 1.0) {
			throw ConfigError(node[i]["probability"],
				"invalid encapsulation probabilities, total probability is over 1.0");
		}
	}
}

} // namespace config
} // namespace generator
