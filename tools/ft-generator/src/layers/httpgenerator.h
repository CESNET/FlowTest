/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief HTTP message generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../pcpppacket.h"

#include <stdint.h>

namespace generator {

/**
 * @brief Generate a random payload
 *
 * @param packet  The packet to append the payload onto
 * @param size    The size of the payload
 */
void HttpGeneratePayload(PcppPacket& packet, uint64_t size);

/**
 * @brief Generate a HTTP GET request
 *
 * @param packet  The packet to append the payload onto
 * @param size    The desired size of the HTTP layer part of the packet
 */
void HttpGenerateGetRequest(PcppPacket& packet, uint64_t size);

/**
 * @brief Generate a HTTP POST request
 *
 * @param packet             The packet to append the payload onto
 * @param size               The desired size of the HTTP layer part of the packet
 * @param contentInclHeader  The full length of the HTTP message, header + content length, used to
 *                           calculate the content-length field
 *
 * @note `size` is the length of the HTTP layer of a SINGLE packet, meanwhile `contentInclHeader` is
 *       the size of the whole HTTP message that can span MULTIPLE packets
 *
 * @throws std::logic_error if the provided size is not big enough to generate a HTTP message
 */
void HttpGeneratePostRequest(PcppPacket& packet, uint64_t size, uint64_t contentLenInclHeader);

/**
 * @brief Generate a HTTP response
 *
 * @param packet             The packet to append the payload onto
 * @param size               The desired size of the HTTP layer part of the packet
 * @param contentInclHeader  The full length of the HTTP message, header + content length, used to
 *                           calculate the content-length field
 *
 * @note `size` is the length of the HTTP layer of a SINGLE packet, meanwhile `contentInclHeader` is
 *       the size of the whole HTTP message that can span MULTIPLE packets
 *
 * @throws std::logic_error if the provided size is not big enough to generate a HTTP message
 */
void HttpGenerateResponse(PcppPacket& packet, uint64_t size, uint64_t contentLenInclHeader);

} // namespace generator
