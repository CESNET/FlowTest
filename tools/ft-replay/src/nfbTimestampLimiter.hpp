/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbTimestampLimiter class.
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
 * @brief Represents a firmware timestamp limiter component.
 */
class NfbTimestampLimiter : public NfbFirmwareCompoment {
public:
	/**
	 * @brief Constructor for NfbTimestampLimiter.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 */
	NfbTimestampLimiter(nfb_device* nfbDevice);

	/**
	 * @brief Validates the timestamp limiter compatibility and format.
	 * @param requiredCompatibleCount The expected number of compatible instances.
	 * @note This function should be called before using other member functions.
	 * @throws std::runtime_error if validation fails.
	 */
	void Validate(size_t requiredCompatibleCount);

	/**
	 * @brief Checks if the timestamp limiter is present.
	 * @return True if the timestamp limiter is present, otherwise false.
	 */
	bool IsPresent() const override;

	/**
	 * @brief Configure the timestamp limiter FW register.
	 */
	void ConfigureTimestampLimiter();

private:
	enum class RegisterOffset : uint32_t {
		resetRegister = 0x00,
		resetSelectQueues = 0x04,
		topSpeedRegister = 0x08,
	};

	void ValidateTimestampFormat();
	void Reset();

	size_t _compatibleCount = 0;
	static const std::string COMPATIBLE_NAME;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbTimestampLimiter");
};

} // namespace replay
