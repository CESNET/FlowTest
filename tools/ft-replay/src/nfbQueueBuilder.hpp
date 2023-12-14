/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbQueueBuilder class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "nfbQueue.hpp"
#include "offloads.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <memory>

namespace replay {

/**
 * @brief Enumerates the possible super packets modes.
 */
enum class SuperPacketsMode {
	Auto, /**< Automatically determine the super packets mode. (enable if supported) */
	Enable,
	Disable,
};

/**
 * @brief Builder class for constructing NfbQueue instances.
 */
class NfbQueueBuilder {
public:
	/**
	 * @brief Constructor for NfbQueueBuilder.
	 */
	NfbQueueBuilder();

	/**
	 * @brief Builds a NfbQueue instance.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 * @param queueId ID of the queue.
	 * @return A unique pointer to the constructed NfbQueue.
	 * @throws std::invalid_argument if the configuration is invalid.
	 */
	std::unique_ptr<NfbQueue> Build(nfb_device* nfbDevice, unsigned queueId);

	/**
	 * @brief Sets the burst size for the NfbQueue.
	 * @param burstSize The burst size to set.
	 */
	void SetBurstSize(size_t burstSize);

	/**
	 * @brief Enables using of the replicator header for the NfbQueue.
	 */
	void EnableReplicatorHeader();

	/**
	 * @brief Sets super packets configuration for the NfbQueue.
	 * @param superPacketSize Size of the super packet.
	 * @param superPacketLimit Limit for super packets.
	 */
	void SetSuperPackets(size_t superPacketSize, size_t superPacketLimit);

private:
	void ValidateOptions() const;
	NfbQueueConfig _queueConfig;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbQueueBuilder");
};

} // namespace replay
