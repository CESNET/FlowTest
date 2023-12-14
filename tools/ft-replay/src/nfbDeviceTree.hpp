/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbDeviceTree class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <memory>
#include <string>

namespace replay {

/**
 * @brief Represents an interface for interacting with the device tree.
 */
class NfbDeviceTree {
public:
	/**
	 * @brief Constructor for NfbDeviceTree.
	 * @param deviceTree Pointer to the device tree.
	 */
	NfbDeviceTree(const void* deviceTree);

	/**
	 * @brief Gets the count of compatible nodes in the device tree.
	 * @param compatiblePath The compatible path to search for.
	 * @return The count of compatible nodes.
	 */
	size_t GetCompatibleCount(const std::string& compatiblePath) const;

	/**
	 * @brief Gets the count of a specific property in compatible nodes.
	 * @param compatiblePath The compatible path to search for.
	 * @param propertyPath The property path to check.
	 * @return The count of the specified property in compatible nodes.
	 */
	size_t GetCompatiblePropertyCount(
		const std::string& compatiblePath,
		const std::string& propertyPath) const;

	/**
	 * @brief Gets the value of a property in a specific compatible node.
	 * @param compatiblePath The compatible path to search for.
	 * @param propertyPath The property path to retrieve.
	 * @param compatibleIndex The index of the compatible node.
	 * @return The value of the specified property.
	 * @throws std::runtime_error if the operation fails.
	 */
	uint32_t GetCompatiblePropertyValue(
		const std::string& compatiblePath,
		const std::string& propertyPath,
		unsigned compatibleIndex) const;

private:
	const void* _deviceTree;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbDeviceTree");
};

} // namespace replay
