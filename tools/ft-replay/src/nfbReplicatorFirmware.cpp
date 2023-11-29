/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbReplicatorFirmware class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbReplicatorFirmware.hpp"

#include <variant>

namespace replay {

NfbReplicatorFirmware::NfbReplicatorFirmware(nfb_device* nfbDevice)
	: _replicatorCore(nfbDevice)
	, _timestampLimiter(nfbDevice)
	, _frameUnpacker(nfbDevice)
	, _rateLimiter(nfbDevice)
{
	size_t replicatorCoreCount = _replicatorCore.GetCoreCount();
	_timestampLimiter.Validate(replicatorCoreCount);
	_frameUnpacker.Validate(replicatorCoreCount);
	_rateLimiter.Validate(replicatorCoreCount);
}

bool NfbReplicatorFirmware::IsReplicatorFirmwareBooted() const
{
	return _replicatorCore.IsPresent();
}

bool NfbReplicatorFirmware::IsSuperPacketsFeatureSupported() const
{
	return _frameUnpacker.IsPresent();
}

size_t NfbReplicatorFirmware::GetSuperPacketsLimit()
{
	if (!_frameUnpacker.IsPresent()) {
		return 0;
	}

	return _frameUnpacker.GetSuperPacketsLimit();
}

Offloads NfbReplicatorFirmware::GetOffloads()
{
	if (!_replicatorCore.IsPresent()) {
		return Offloads {};
	}

	Offloads offloads = 0;

	if (_replicatorCore.IsChecksumOffloadSupported()) {
		offloads |= Offload::CHECKSUM_IPV4;
		offloads |= Offload::CHECKSUM_UDP;
		offloads |= Offload::CHECKSUM_TCP;
		offloads |= Offload::CHECKSUM_ICMPV6;
	}

	if (_timestampLimiter.IsPresent()) {
		offloads |= Offload::RATE_LIMIT_TIME;
	}

	if (_rateLimiter.IsPresent()) {
		offloads |= Offload::RATE_LIMIT_PACKETS;
		offloads |= Offload::RATE_LIMIT_BYTES;
	}

	return offloads;
}

void NfbReplicatorFirmware::ConfigureLimiter(const Config::RateLimit& rateLimitConfig)
{
	if (std::holds_alternative<Config::RateLimitPps>(rateLimitConfig)) {
		bool isPacketLimit = true;
		_rateLimiter.ConfigureRateLimiter(
			std::get<Config::RateLimitPps>(rateLimitConfig).value,
			isPacketLimit);
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimitConfig)) {
		bool isPacketLimit = false;
		_rateLimiter.ConfigureRateLimiter(
			std::get<Config::RateLimitMbps>(rateLimitConfig).ConvertToBytesPerSecond(),
			isPacketLimit);
	} else if (std::holds_alternative<Config::RateLimitTimeUnit>(rateLimitConfig)) {
		_timestampLimiter.ConfigureTimestampLimiter();
	}
}

} // namespace replay
