/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbTimestampLimiter class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbTimestampLimiter.hpp"

#include <stdexcept>

namespace replay {

const std::string NfbTimestampLimiter::COMPATIBLE_NAME = "cesnet,ofm,timestamp_limiter";

NfbTimestampLimiter::NfbTimestampLimiter(nfb_device* nfbDevice)
	: NfbFirmwareCompoment(COMPATIBLE_NAME, nfbDevice)
{
}

void NfbTimestampLimiter::Validate(size_t requiredCompatibleCount)
{
	size_t compatibleCount = _compatible.GetCompatibleCount();
	_compatibleCount = compatibleCount;
	if (!compatibleCount) {
		return;
	}

	if (compatibleCount != requiredCompatibleCount) {
		_logger->error(
			"Invalid number of timestamp limiter compatible instances. Expected: {}, actual: {}",
			requiredCompatibleCount,
			compatibleCount);
		throw std::runtime_error("NfbTimestampLimiter::Validate() has failed");
	}

	ValidateTimestampFormat();
	Reset();
}

void NfbTimestampLimiter::ValidateTimestampFormat()
{
	const uint32_t requiredTimestampFormat = 1;
	const std::string timestampFormatPropertyName = "timestamp_format";

	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		uint32_t timestampFormat
			= _compatible.GetPropertyValue(timestampFormatPropertyName, compatibleIndex);
		if (timestampFormat != requiredTimestampFormat) {
			_logger->error(
				"Invalid timestamp format. Expected: {}, actual: {}",
				requiredTimestampFormat,
				timestampFormat);
			throw std::runtime_error("NfbTimestampLimiter::ValidateTimestampFormat() has failed");
		}
	}
}

bool NfbTimestampLimiter::IsPresent() const
{
	return _compatible.GetCompatibleCount();
}

void NfbTimestampLimiter::Reset()
{
	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::topSpeedRegister),
			0x1,
			compatibleIndex);
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::resetRegister),
			0x1,
			compatibleIndex);
	}
}

void NfbTimestampLimiter::ConfigureTimestampLimiter()
{
	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::resetRegister),
			0x1,
			compatibleIndex);
		_compatible.SetRegisterValue(
			static_cast<uint32_t>(RegisterOffset::topSpeedRegister),
			0x0,
			compatibleIndex);
	}
}

} // namespace replay
