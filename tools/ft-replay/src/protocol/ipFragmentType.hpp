/**
 * @file
 * @brief IP Fragment type
 * @author Lukas Hutak <hutak@cesnet.cz>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

namespace replay::protocol {

/**
 * @brief Type of IPv4/IPv6 fragment.
 */
enum class IPFragmentType {
	None, /**< Not fragmented */
	First, /**< Fragmented, the first fragment */
	Middle, /**< Fragmented, neither the first nor the last fragment */
	Last, /**< Fragmented, the last fragment */
};

} // namespace replay::protocol
