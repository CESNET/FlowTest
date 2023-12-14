/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbFirmwareComponent class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "nfbCompatible.hpp"
#include "nfbDeviceTree.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <string>

namespace replay {

/**
 * @brief Abstract base class for firmware components in NFB devices.
 *
 * This class provides a common interface for firmware components and includes
 * functionality to check the presence of a component. It also encapsulates
 * compatibility information and the device tree for the associated NFB device.
 */
class NfbFirmwareCompoment {
public:
	/**
	 * @brief Constructor for NfbFirmwareComponent.
	 *
	 * @param compatibleName The name indicating compatibility.
	 * @param nfbDevice A pointer to the associated nfb_device.
	 */
	NfbFirmwareCompoment(const std::string& compatibleName, nfb_device* nfbDevice)
		: _compatible(compatibleName, nfbDevice)
		, _deviceTree(nfbDevice)
	{
	}

	/**
	 * @brief Pure virtual function to check the presence of the firmware component.
	 *
	 * @return true if the component is present, false otherwise.
	 */
	virtual bool IsPresent() const = 0;

protected:
	NfbCompatible _compatible; /**< Compatibility information for the firmware component. */
	NfbDeviceTree _deviceTree; /**< Device tree information for the associated NFB device. */
};

} // namespace replay
