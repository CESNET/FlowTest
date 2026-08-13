/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief Constants of SSH
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
namespace generator {
// Packet counts
static constexpr uint8_t SSH_MIN_DIR_PKT_CNT = 4;
static constexpr uint8_t SSH_MIN_KEX_PKT_CNT = 5;
static constexpr uint8_t SSH_MIN_SUCCESS_PKT_CNT = 10;

// Message codes
static constexpr uint8_t SSH_KEX_INIT_MSG_CODE = 20;
static constexpr uint8_t SSH_ECDH_CLIENT_MSG_CODE = 30;
static constexpr uint8_t SSH_ECDH_SERVER_MSG_CODE = 31;
static constexpr uint8_t SSH_NEW_KEYS_MSG_CODE = 21;

// Zeros
static constexpr uint8_t SSH_DEFAULT_LANGUAGE_CNT = 0;
static constexpr uint8_t SSH_DEFAULT_FIRST_PACKET_CNT = 0;

// Byte counts
static constexpr uint8_t SSH_COOKIE_BYTES_CNT = 16;
static constexpr uint8_t SSH_DEFAULT_SIZE_BYTES = 4;
static constexpr uint8_t SSH_AES_BLOCK_SIZE = 16;
static constexpr uint8_t SSH_KEX_BLOCK_SIZE = 8;
static constexpr uint8_t SSH_PACKET_PADDING_INFO_SIZE = 1;
static constexpr uint8_t SSH_MIN_PADDING_SIZE = 4;
static constexpr uint16_t SSH_MAX_KEX_BYTES_PER_DIR = 3000;
static constexpr uint64_t SSH_MAX_ALGORITHM_PAYLOAD_SIZE = 32768;
static constexpr uint64_t SSH_MIN_ALGORITHM_PAYLOAD_SIZE = 94;
static constexpr uint8_t SSH_PACKET_SIZEINFO_SIZE = 4;
static constexpr uint8_t SSH_KEX_INIT_MSG_SIZE = 1;
static constexpr uint32_t WIRESHARK_MAX_BYTES = 25000;
} // namespace generator
