/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbRateLimiter class.
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
 * @brief Firmware component representing a rate limiter.
 */
class NfbRateLimiter : public NfbFirmwareCompoment {
public:
	/**
	 * @brief Constructor for NfbRateLimiter.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 */
	NfbRateLimiter(nfb_device* nfbDevice);

	/**
	 * @brief Validates the number of compatible instances.
	 *
	 * @param requiredCompatibleCount The expected number of compatible instances.
	 * @note This function should be called before using other member functions.
	 * @throws std::runtime_error if the number of compatible instances is invalid.
	 */
	void Validate(size_t requiredCompatibleCount);

	/**
	 * @brief Checks if the rate limiter is present.
	 *
	 * @return true if the rate limiter is present, false otherwise.
	 */
	bool IsPresent() const override;

	/**
	 * @brief Configures the rate limiter.
	 *
	 * @param limitPerSecond The limit per second for the rate limiter.
	 * @param isPacketLimit Flag indicating whether the limit is in bytes or packets.
	 */
	void ConfigureRateLimiter(uint64_t limitPerSecond, bool isPacketLimit);

private:
	enum class RegisterOffset : uint32_t {
		status = 0x00,
		sectionLength = 0x04,
		intervalLength = 0x08,
		intervalCount = 0x0c,
		frequency = 0x10,
		speed = 0x14,
	};

	enum class StatusRegisterFlag : uint32_t {
		reset = 0x00,
		idle = 0x01,
		configuration = 0x02,
		trafficShaping = 0x04,
		auxiliary = 0x08,
		resetPointer = 0x10,
		limitingType = 0x20, // 0 - bytes, 1 - packets
	};

	uint32_t ConvertLimitPerSecondsToSpeedRegisterValue(
		uint64_t limitPerSecond,
		bool isPacketLimit,
		unsigned compatibleIndex);
	uint32_t CalculateSectionLengthRegisterValue(
		uint64_t limitPerSecond,
		bool isPacketLimit,
		unsigned compatibleIndex);
	void SetDefaultValuesToRegisters();
	void Reset();

	size_t _compatibleCount = 0;
	static const std::string COMPATIBLE_NAME;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbRateLimiter");
};

} // namespace replay
