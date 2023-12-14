/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbFrameUnpacker class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "nfbFirmwareComponent.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <memory>
#include <string>

namespace replay {

/**
 * @brief Firmware component representing a frame unpacker (super packets).
 */
class NfbFrameUnpacker : public NfbFirmwareCompoment {
public:
	/**
	 * @brief Constructor for NfbFrameUnpacker.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 */
	NfbFrameUnpacker(nfb_device* nfbDevice);

	/**
	 * @brief Validates the frame unpacker compatibility.
	 * @param requiredCompatibleCount The expected number of compatible instances.
	 * @note This function should be called before using other member functions.
	 * @throws std::runtime_error if validation fails.
	 */
	void Validate(size_t requiredCompatibleCount);

	/**
	 * @brief Checks if the frame unpacker is present.
	 * @return True if the frame unpacker is present, otherwise false.
	 */
	bool IsPresent() const override;

	/**
	 * @brief Gets the super packets limit property.
	 * @return The super packets limit value.
	 * @throws std::runtime_error if the property values differ among compatibles.
	 */
	size_t GetSuperPacketsLimit() const;

private:
	size_t _compatibleCount = 0;

	static const std::string COMPATIBLE_NAME;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbFrameUnpacker");
};

} // namespace replay
