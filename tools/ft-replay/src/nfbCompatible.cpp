/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbCompatible class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbCompatible.hpp"

#include <stdexcept>

namespace replay {

NfbCompatible::NfbCompatible(const std::string& compatibleName, nfb_device* nfbDevice)
	: _compatibleName(compatibleName)
	, _deviceTree(nfb_get_fdt(nfbDevice))
	, _nfbDevice(nfbDevice)
{
}

uint32_t NfbCompatible::GetRegisterValue(int registerOffset, unsigned compatibleIndex)
{
	uint32_t value;
	Compatible compatible = OpenCompatible(compatibleIndex);
	auto readSize = nfb_comp_read(compatible.get(), &value, sizeof(value), registerOffset);
	if (readSize != sizeof(uint32_t)) {
		_logger->error("Failed to read register value from compatible '{}'", _compatibleName);
		throw std::runtime_error("NfbCompatible::GetRegisterValue() has failed");
	}

	return value;
}

void NfbCompatible::SetRegisterValue(
	int registerOffset,
	uint32_t valueToSet,
	unsigned compatibleIndex)
{
	Compatible compatible = OpenCompatible(compatibleIndex);
	auto writeSize
		= nfb_comp_write(compatible.get(), &valueToSet, sizeof(valueToSet), registerOffset);
	if (writeSize != sizeof(uint32_t)) {
		_logger->error("Failed to write register value to compatible '{}'", _compatibleName);
		throw std::runtime_error("NfbCompatible::SetRegisterValue() has failed");
	}
}

uint32_t
NfbCompatible::GetPropertyValue(const std::string& propertyName, unsigned compatibleIndex) const
{
	return _deviceTree.GetCompatiblePropertyValue(_compatibleName, propertyName, compatibleIndex);
}

uint32_t NfbCompatible::GetCompatibleCount() const
{
	return _deviceTree.GetCompatibleCount(_compatibleName);
}

uint32_t NfbCompatible::GetCompatiblePropertyCount(const std::string& propertyName) const
{
	return _deviceTree.GetCompatiblePropertyCount(_compatibleName, propertyName);
}

NfbCompatible::Compatible NfbCompatible::OpenCompatible(unsigned compatibleIndex)
{
	int compatibleOffset = nfb_comp_find(_nfbDevice, _compatibleName.c_str(), compatibleIndex);
	if (compatibleOffset < 0) {
		_logger->error(
			"Failed to find compatible '{}' with index '{}'",
			_compatibleName,
			compatibleIndex);
		throw std::runtime_error("NfbCompatible::OpenCompatible() has failed");
	}

	Compatible compatible = {nullptr, &nfb_comp_close};
	compatible.reset(nfb_comp_open(_nfbDevice, compatibleOffset));
	if (compatible == nullptr) {
		_logger->error("Failed to open compatible '{}'", _compatibleName);
		throw std::runtime_error("NfbCompatible::OpenCompatible() has failed");
	}
	return compatible;
}

} // namespace replay
