/**
 * @file
 * @brief EthTool class
 * @author Matej Hulak <hulak@cesnet.cz>
 * @brief Ethtool C API
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"

#include <memory>
#include <string_view>

namespace replay {

class EthTool {
public:
	struct Driver {
		std::string _driver;
		std::string _version;
		std::string _fwVersion;
		std::string _busInfo;
	};

	struct Channels {
		uint32_t _txCount;
		uint32_t _maxTxCount;
		uint32_t _combinedCount;
		uint32_t _maxCombinedCount;
	};

	/**
	 * @brief EthTool constructor
	 *
	 * @param[in] Interface name
	 */
	explicit EthTool(std::string_view ifcName);

	EthTool(const EthTool&) = delete;
	EthTool(EthTool&&) = delete;
	EthTool& operator=(const EthTool&) = delete;
	EthTool& operator=(EthTool&&) = delete;

	/**
	 * @brief Default destructor
	 */
	virtual ~EthTool() = default;

	/**
	 * @brief Get network device state
	 *
	 * @return true if device up, false otherwise
	 */
	bool GetDeviceState();

	/**
	 * @brief Get card driver info
	 *
	 * @return Structure with driver info
	 */
	struct Driver GetDriver();

	/**
	 * @brief Get card channels count
	 *
	 * @return Structure with channels counts
	 */
	struct Channels GetChannels();

protected:
	int ProcessRequest(void* eth_data);

private:
	std::string _ifcName;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("EthTool");
};

} // namespace replay
