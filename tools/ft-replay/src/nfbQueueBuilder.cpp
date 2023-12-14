/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbQueueBuilder.hpp"

#include <stdexcept>

namespace replay {

NfbQueueBuilder::NfbQueueBuilder()
{
	_queueConfig = {};
}

std::unique_ptr<NfbQueue> NfbQueueBuilder::Build(nfb_device* nfbDevice, unsigned queueId)
{
	ValidateOptions();

	return std::make_unique<NfbQueue>(_queueConfig, nfbDevice, queueId);
}

void NfbQueueBuilder::ValidateOptions() const
{
	if (_queueConfig.superPacketsEnabled && !_queueConfig.replicatorHeaderEnabled) {
		_logger->error("Invalid configuration: super packets require replicator header");
		throw std::invalid_argument("NfbQueueBuilder::ValidateOptions() has failed");
	}
}

void NfbQueueBuilder::SetBurstSize(size_t burstSize)
{
	_queueConfig.maxBurstSize = burstSize;
}

void NfbQueueBuilder::EnableReplicatorHeader()
{
	_queueConfig.replicatorHeaderEnabled = true;
}

void NfbQueueBuilder::SetSuperPackets(size_t superPacketSize, size_t superPacketLimit)
{
	_queueConfig.superPacketSize = superPacketSize;
	_queueConfig.superPacketLimit = superPacketLimit;
	_queueConfig.superPacketsEnabled = true;
}

} // namespace replay
