/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Output plugin interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace replay {

class OutputQueue;

/**
 * @brief Output plugin interface
 *
 */
struct OutputPlugin {

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	virtual size_t GetQueueCount() const noexcept = 0;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	virtual OutputQueue* GetQueue(uint16_t queueId) = 0;

	/**
	 * @brief Default virtual destructor
	 */
	virtual ~OutputPlugin() = default;
};

} // namespace replay
