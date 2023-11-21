/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbReplicatorCore class.
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
 * @brief Represents a core component of the NFB replicator.
 */
class NfbReplicatorCore : public NfbFirmwareCompoment {
public:
	/**
	 * @brief Constructor for NfbReplicatorCore.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 */
	NfbReplicatorCore(nfb_device* nfbDevice);

	/**
	 * @brief Gets the count of replicator cores.
	 * @return The count of replicator cores.
	 */
	size_t GetCoreCount() const;

	/**
	 * @brief Checks if the replicator core is present.
	 * @return True if the replicator core is present, otherwise false.
	 */
	bool IsPresent() const override;

	/**
	 * @brief Checks if checksum offload is supported in all replicator cores.
	 * @return True if checksum offload is supported in all replicator cores, otherwise false.
	 */
	bool IsChecksumOffloadSupported() const;

private:
	void ValidateTimestampFormat();

	size_t _coreCount = 0;
	static const std::string COMPATIBLE_NAME;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbReplicatorCore");
};

} // namespace replay
