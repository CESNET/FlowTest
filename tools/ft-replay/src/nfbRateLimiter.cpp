/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbRateLimiter class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbRateLimiter.hpp"

#include <cmath>
#include <stdexcept>

namespace replay {

const std::string NfbRateLimiter::COMPATIBLE_NAME = "cesnet,ofm,rate_limiter";

NfbRateLimiter::NfbRateLimiter(nfb_device* nfbDevice)
	: NfbFirmwareCompoment(COMPATIBLE_NAME, nfbDevice)
{
}

void NfbRateLimiter::Validate(size_t requiredCompatibleCount)
{
	size_t compatibleCount = _compatible.GetCompatibleCount();
	_compatibleCount = compatibleCount;
	if (!compatibleCount) {
		return;
	}

	if (compatibleCount != requiredCompatibleCount) {
		_logger->error(
			"Invalid number of compatible instances. Expected: {}, actual: {}",
			requiredCompatibleCount,
			compatibleCount);
		throw std::runtime_error("NfbRateLimiter::Validate() has failed");
	}

	Reset();
}

void NfbRateLimiter::Reset()
{
	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::status),
			static_cast<uint32_t>(StatusRegisterFlag::reset),
			compatibleIndex);
	}
}

void NfbRateLimiter::ConfigureRateLimiter(uint64_t limitPerSecond, bool isPacketLimit)
{
	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::status),
			static_cast<uint32_t>(StatusRegisterFlag::configuration),
			compatibleIndex);

		uint32_t speedRegisterValue = ConvertLimitPerSecondsToSpeedRegisterValue(
			limitPerSecond,
			isPacketLimit,
			compatibleIndex);
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::speed),
			speedRegisterValue,
			compatibleIndex);

		/**
		 * Setting the traffic limiting according to packets or bytes is done by setting the
		 * combination of the "auxiliary" flag and the value of the fifth bit, which should be
		 * zero for bytes and non-zero for packets.
		 */
		if (isPacketLimit) {
			_compatible.SetRegisterValue(
				static_cast<uint32_t>(RegisterOffset::status),
				static_cast<uint32_t>(StatusRegisterFlag::auxiliary)
					| static_cast<uint32_t>(StatusRegisterFlag::limitingType),
				compatibleIndex);
		} else {
			_compatible.SetRegisterValue(
				static_cast<uint32_t>(RegisterOffset::status),
				static_cast<uint32_t>(StatusRegisterFlag::auxiliary),
				compatibleIndex);
		}
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::status),
			static_cast<uint32_t>(StatusRegisterFlag::trafficShaping),
			compatibleIndex);
	}

	// use this sleep to make sure that the rate limiter is configured before the first packet
	// arrives
	sleep(4);
}

uint32_t NfbRateLimiter::ConvertLimitPerSecondsToSpeedRegisterValue(
	uint64_t limitPerSecond,
	bool isPacketLimit,
	unsigned compatibleIndex)
{
	uint32_t frequency = _compatible.GetRegisterValue(
		static_cast<uint32_t>(RegisterOffset::frequency),
		compatibleIndex);
	uint32_t sectionLength
		= CalculateSectionLengthRegisterValue(limitPerSecond, isPacketLimit, compatibleIndex);

	uint32_t ticksPerSecond = frequency * 1000000;
	float sectionsPerSecond = float(ticksPerSecond) / float(sectionLength);
	uint32_t speedRegisterValue = std::ceil(float(limitPerSecond / sectionsPerSecond));

	return speedRegisterValue;
}

uint32_t NfbRateLimiter::CalculateSectionLengthRegisterValue(
	uint64_t limitPerSecond,
	bool isPacketLimit,
	unsigned compatibleIndex)
{
	uint32_t frequency = _compatible.GetRegisterValue(
		static_cast<uint32_t>(RegisterOffset::frequency),
		compatibleIndex);
	const std::string defaultSectionLengthPropertyName = "default_section_len";
	uint32_t defaultSectionLength
		= _compatible.GetPropertyValue(defaultSectionLengthPropertyName, compatibleIndex);

	uint32_t ticksPerSecond = frequency * 1000000;
	uint64_t minimumSpeed;

	/*
	 * The value of speed register cannot be 1 for packet or <= 192 for bytes, so we have to
	 * change the section length to prevent speed register from being in this range.
	 */
	if (isPacketLimit) {
		minimumSpeed = 2;
	} else {
		minimumSpeed = 193;
	}

	uint32_t newSectionLength = minimumSpeed * ticksPerSecond / limitPerSecond;
	if (defaultSectionLength <= newSectionLength) {
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::sectionLength),
			newSectionLength,
			compatibleIndex);
		return newSectionLength;
	}

	_compatible.SetRegisterValue(
		static_cast<uint32_t>(RegisterOffset::sectionLength),
		defaultSectionLength,
		compatibleIndex);

	return defaultSectionLength;
}

bool NfbRateLimiter::IsPresent() const
{
	return _compatibleCount;
}

} // namespace replay
