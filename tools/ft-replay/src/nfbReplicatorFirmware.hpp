/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbReplicatorHeader structure and NfbReplicatorFirmware class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config.hpp"
#include "nfbFrameUnpacker.hpp"
#include "nfbRateLimiter.hpp"
#include "nfbReplicatorCore.hpp"
#include "nfbTimestampLimiter.hpp"
#include "offloads.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <array>
#include <memory>
#include <stdint.h>

namespace replay {

/**
 * @brief Represents the packet header structure for NFB replicator firmware.
 */
struct NfbReplicatorHeader {
	uint16_t length; /**< Total length in bytes of the packet. */
	uint16_t l2Length : 7; /**< Length of Layer 2 (L2) in bytes. */
	uint16_t l3Length : 9; /**< Length of Layer 3 (L3) in bytes. */
	uint8_t calculateL3Checksum : 1; /**< Flag indicating whether to calculate L3 checksum. */
	uint8_t calculateL4Checksum : 1; /**< Flag indicating whether to calculate L4 checksum. */
	// cppcheck-suppress syntaxError
	enum class L3Type : uint8_t {
		IPv4 = 0, /**< Indicates IPv4 protocol. */
		IPv6 = 1, /**< Indicates IPv6 protocol. */
	} l3Type : 1; /**< Enum representing the Layer 3 (L3) protocol type. */
	enum class L4Type : uint8_t {
		UDP = 0x0, /**< Indicates UDP protocol. */
		TCP = 0x1, /**< Indicates TCP protocol. */
		ICMPv6 = 0x2, /**< Indicates ICMPv6 protocol. */
	} l4Type : 2; /**< Enum representing the Layer 4 (L4) protocol type. */
	uint8_t reserved : 3;
	std::array<uint8_t, 6> timestamp; /**< Array representing the timestamp. */
	std::array<uint8_t, 5> unused;
} __attribute__((packed));

static_assert(sizeof(NfbReplicatorHeader) == 16, "Invalid header definition");

/**
 * @brief Represents the firmware interface for the NFB replicator.
 *
 * The class provides methods to interact with different components of the NFB replicator firmware.
 */
class NfbReplicatorFirmware {
public:
	/**
	 * @brief Constructor for NfbReplicatorFirmware.
	 * @param nfbDevice Pointer to the associated nfb_device.
	 */
	NfbReplicatorFirmware(nfb_device* nfbDevice);

	/**
	 * @brief Checks if the NFB replicator firmware is booted.
	 * @return true if the firmware is booted, false otherwise.
	 */
	bool IsReplicatorFirmwareBooted() const;

	/**
	 * @brief Checks if the super packets feature is supported.
	 * @return true if the feature is supported, false otherwise.
	 */
	bool IsSuperPacketsFeatureSupported() const;

	/**
	 * @brief Gets the limit for super packets.
	 * @return The limit for super packets if supported, otherwise 0.
	 */
	size_t GetSuperPacketsLimit();

	/**
	 * @brief Gets the supported offloads for the NFB replicator firmware.
	 * @return The supported offloads.
	 */
	Offloads GetOffloads();

	/**
	 * @brief Configures the limiter based on the provided rate limit configuration.
	 * @param rateLimitConfig The rate limit configuration.
	 */
	void ConfigureLimiter(const Config::RateLimit& rateLimitConfig);

private:
	NfbReplicatorCore _replicatorCore;
	NfbTimestampLimiter _timestampLimiter;
	NfbFrameUnpacker _frameUnpacker;
	NfbRateLimiter _rateLimiter;
};

} // namespace replay
