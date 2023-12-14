/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbDeviceTree class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbDeviceTree.hpp"

extern "C" {
#include <libfdt.h>
#include <nfb/ndp.h>
}

#include <stdexcept>

namespace replay {

NfbDeviceTree::NfbDeviceTree(const void* deviceTree)
	: _deviceTree(deviceTree)
{
}

size_t NfbDeviceTree::GetCompatibleCount(const std::string& compatiblePath) const
{
	int nodeOffset;
	size_t count = 0;

	nodeOffset = fdt_node_offset_by_compatible(_deviceTree, -1, compatiblePath.c_str());
	while (nodeOffset != -FDT_ERR_NOTFOUND) {
		if (nodeOffset < 0) {
			_logger->error("Invalid compatible node offset {}", nodeOffset);
			throw std::runtime_error("NfbDeviceTree::GetCompatibleCount() has failed");
		}
		count++;
		nodeOffset = fdt_node_offset_by_compatible(_deviceTree, nodeOffset, compatiblePath.c_str());
	}

	return count;
}

size_t NfbDeviceTree::GetCompatiblePropertyCount(
	const std::string& compatiblePath,
	const std::string& propertyPath) const
{
	int nodeOffset;
	size_t count = 0;

	nodeOffset = fdt_node_offset_by_compatible(_deviceTree, -1, compatiblePath.c_str());
	while (nodeOffset != -FDT_ERR_NOTFOUND) {
		if (nodeOffset < 0) {
			_logger->error("Invalid compatible node offset {}", nodeOffset);
			throw std::runtime_error("NfbDeviceTree::GetCompatiblePropertyCount() has failed");
		}
		const struct fdt_property* property
			= fdt_get_property(_deviceTree, nodeOffset, propertyPath.c_str(), nullptr);
		if (property != nullptr) {
			count++;
		}
		nodeOffset = fdt_node_offset_by_compatible(_deviceTree, nodeOffset, compatiblePath.c_str());
	}

	return count;
}

uint32_t NfbDeviceTree::GetCompatiblePropertyValue(
	const std::string& compatiblePath,
	const std::string& propertyPath,
	unsigned compatibleIndex) const
{
	const size_t requiredPropertyLength = sizeof(uint32_t);
	int nodeOffset;
	unsigned currentIndex = 0;

	nodeOffset = fdt_node_offset_by_compatible(_deviceTree, -1, compatiblePath.c_str());
	while (nodeOffset != -FDT_ERR_NOTFOUND) {
		if (currentIndex++ != compatibleIndex) {
			nodeOffset
				= fdt_node_offset_by_compatible(_deviceTree, nodeOffset, compatiblePath.c_str());
			continue;
		}
		int propertyLength;
		const struct fdt_property* property
			= fdt_get_property(_deviceTree, nodeOffset, propertyPath.c_str(), &propertyLength);
		if (property == nullptr) {
			_logger->error("Property {} not found", propertyPath);
			throw std::runtime_error("NfbDeviceTree::GetCompatiblePropertyValue() has failed");
		}
		if (propertyLength != requiredPropertyLength) {
			_logger->error(
				"Property {} has invalid length {} (expected {})",
				propertyPath,
				propertyLength,
				requiredPropertyLength);
			throw std::runtime_error("NfbDeviceTree::GetCompatiblePropertyValue() has failed");
		}
		uint32_t propertyValue = fdt32_to_cpu(*reinterpret_cast<const uint32_t*>(property->data));
		return propertyValue;
	}
	_logger->error("Compatible {} not found", compatiblePath);
	throw std::runtime_error("NfbDeviceTree::GetCompatiblePropertyValue() has failed");
}

} // namespace replay
