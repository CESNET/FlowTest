/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbCompatible class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "nfbDeviceTree.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <memory>
#include <string>

namespace replay {

/**
 * @brief Represents a wrapper for interacting with device tree compatible.
 */
class NfbCompatible {
public:
	/**
	 * @brief Constructor for NfbCompatible.
	 * @param compatibleName The name of the compatible.
	 * @param nfbDevice Pointer to the nfb device.
	 */
	NfbCompatible(const std::string& compatibleName, nfb_device* nfbDevice);

	/**
	 * @brief Retrieves the value of a register from the specified nfb compatible.
	 * @param registerOffset The offset of the register.
	 * @param compatibleIndex The index of the compatible instance.
	 * @return The value of the register.
	 * @throws std::runtime_error if the operation fails.
	 */
	uint32_t GetRegisterValue(int registerOffset, unsigned compatibleIndex);

	/**
	 * @brief Sets the value of a register in the specified nfb compatible.
	 * @param registerOffset The offset of the register.
	 * @param valueToSet The value to write to the register.
	 * @param compatibleIndex The index of the compatible instance.
	 * @throws std::runtime_error if the operation fails.
	 */
	void SetRegisterValue(int registerOffset, uint32_t valueToSet, unsigned compatibleIndex);

	/**
	 * @brief Retrieves the value of a property from the device tree.
	 * @param propertyName The name of the property.
	 * @param compatibleIndex The index of the compatible instance.
	 * @return The value of the property.
	 */
	uint32_t GetPropertyValue(const std::string& propertyName, unsigned compatibleIndex) const;

	/**
	 * @brief Gets the total number of compatible instances with the specified name.
	 * @return The number of compatible instances.
	 */
	uint32_t GetCompatibleCount() const;

	/**
	 * @brief Gets the number of properties with the specified name for a compatible instances.
	 * @param propertyName The name of the property.
	 * @return The number of properties.
	 */
	uint32_t GetCompatiblePropertyCount(const std::string& propertyName) const;

private:
	using Compatible = std::unique_ptr<struct nfb_comp, decltype(&nfb_comp_close)>;
	Compatible OpenCompatible(unsigned compatibleIndex);

	const std::string _compatibleName;
	NfbDeviceTree _deviceTree;
	nfb_device* _nfbDevice;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbCompatible");
};

} // namespace replay
