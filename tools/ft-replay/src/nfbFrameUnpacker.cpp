/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbFrameUnpacker class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbFrameUnpacker.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace replay {

const std::string NfbFrameUnpacker::COMPATIBLE_NAME = "cesnet,ofm,frame_unpacker";

NfbFrameUnpacker::NfbFrameUnpacker(nfb_device* nfbDevice)
	: NfbFirmwareCompoment(COMPATIBLE_NAME, nfbDevice)
{
}

void NfbFrameUnpacker::Validate(size_t requiredCompatibleCount)
{
	size_t compatibleCount = _compatible.GetCompatibleCount();
	_compatibleCount = compatibleCount;
	if (!compatibleCount) {
		return;
	}

	if (compatibleCount != requiredCompatibleCount) {
		_logger->error(
			"Invalid number of frame unpacker compatible instances. Expected: {}, actual: {}",
			requiredCompatibleCount,
			compatibleCount);
		throw std::runtime_error("NfbFrameUnpacker::Validate() has failed");
	}
}

bool NfbFrameUnpacker::IsPresent() const
{
	return _compatibleCount;
}

size_t NfbFrameUnpacker::GetSuperPacketsLimit() const
{
	std::optional<uint32_t> propertyValue = std::nullopt;
	const std::string superPacketsPropertyName = "unpack_limit";

	for (size_t compatibleIndex = 0; compatibleIndex < _compatibleCount; compatibleIndex++) {
		uint32_t superPacketsLimit
			= _compatible.GetPropertyValue(superPacketsPropertyName, compatibleIndex);
		if (!propertyValue.has_value()) {
			propertyValue = superPacketsLimit;
		} else if (*propertyValue != superPacketsLimit) {
			_logger->error(
				"Property '{}' has different values in different compatibles",
				superPacketsPropertyName);
			throw std::runtime_error("NfbFrameUnpacker::GetSuperPacketsLimit() has failed");
		}
	}

	if (!propertyValue.has_value()) {
		_logger->error("Property '{}' not found", superPacketsPropertyName);
		throw std::runtime_error("NfbFrameUnpacker::GetSuperPacketsLimit() has failed");
	}

	return *propertyValue;
}

} // namespace replay
