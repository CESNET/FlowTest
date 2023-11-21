/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbReplicatorCore class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbReplicatorCore.hpp"

#include <stdexcept>

namespace replay {

const std::string NfbReplicatorCore::COMPATIBLE_NAME = "cesnet,replicator,app_core";

NfbReplicatorCore::NfbReplicatorCore(nfb_device* nfbDevice)
	: NfbFirmwareCompoment(COMPATIBLE_NAME, nfbDevice)
{
	_coreCount = GetCoreCount();
}

size_t NfbReplicatorCore::GetCoreCount() const
{
	return _compatible.GetCompatibleCount();
}

bool NfbReplicatorCore::IsPresent() const
{
	return _coreCount;
}

bool NfbReplicatorCore::IsChecksumOffloadSupported() const
{
	const std::string checksumPropertyName = "checksum_offload";
	size_t propertyCount = _compatible.GetCompatiblePropertyCount(checksumPropertyName);
	if (propertyCount != _coreCount) {
		_logger->warn("Checksum property is not present in all replicator cores");
		return false;
	}
	return propertyCount;
}

} // namespace replay
