/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Encapsulation configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace generator {
namespace config {

/**
 * @brief MPLS encapsulation config
 *
 */
class EncapsulationLayerMpls {
public:
	/**
	 * @brief Construct a new Encapsulation Layer Mpls object from a yaml node
	 */
	EncapsulationLayerMpls(const YAML::Node& node);

	/**
	 * @brief Get the mpls label
	 */
	uint32_t GetLabel() const { return _label; }

private:
	uint32_t _label;
};

/**
 * @brief VLAN encapsulation config
 *
 */
class EncapsulationLayerVlan {
public:
	/**
	 * @brief Construct a new Encapsulation Layer Vlan object from a yaml node
	 */
	EncapsulationLayerVlan(const YAML::Node& node);

	/**
	 * @brief Get the vlan id
	 */
	uint16_t GetId() const { return _id; }

private:
	uint16_t _id;
};

/**
 * @brief Encapsulation layer representation
 *
 */
class EncapsulationLayer
	: public std::variant<std::monostate, EncapsulationLayerMpls, EncapsulationLayerVlan> {
public:
	/**
	 * @brief Construct a new Encapsulation Layer object from a yaml node
	 */
	EncapsulationLayer(const YAML::Node& node);

private:
	using std::variant<std::monostate, EncapsulationLayerMpls, EncapsulationLayerVlan>::variant;
};

/**
 * @brief Representation of a possible encapsulation stack
 *
 */
class EncapsulationVariant {
public:
	/**
	 * @brief Construct a new Encapsulation Variant object from a yaml node
	 */
	EncapsulationVariant(const YAML::Node& node);

	/**
	 * @brief Get the encapsulation layer stack
	 */
	const std::vector<EncapsulationLayer>& GetLayers() const { return _layers; }

	/**
	 * @brief Get the probability of this encapsulation variant
	 */
	double GetProbability() const { return _probability; }

private:
	std::vector<EncapsulationLayer> _layers;
	double _probability;
};

/**
 * @brief Representation of the encapsulation configuration section
 */
class Encapsulation {
public:
	/**
	 * @brief Construct a new Encapsulation object
	 */
	Encapsulation() {}

	/**
	 * @brief Construct a new Encapsulation object from a yaml node
	 */
	Encapsulation(const YAML::Node& node);

	/**
	 * @brief Get the encapsulation variants
	 */
	const std::vector<EncapsulationVariant>& GetVariants() const { return _variants; }

private:
	std::vector<EncapsulationVariant> _variants;
};

} // namespace config
} // namespace generator
