/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Declaration of the NfbPlugin class
 *
 * This file contains the declaration of the NfbPlugin class, which is a plugin
 * for handling Network Feedback (NFB). It extends the OutputPlugin class and
 * provides functionalities for interacting with NFB devices, configuring queues,
 * and managing offloads.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "logger.h"
#include "nfbQueue.hpp"
#include "nfbQueueBuilder.hpp"
#include "offloads.hpp"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"

extern "C" {
#include <nfb/nfb.h>
}

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace replay {

struct NfbPluginConfig {
	std::string deviceName;
	size_t queueCount = 0;
	size_t maxBurstSize = 64;
	size_t packetSize = 0;
	SuperPacketsMode superPacketsMode = SuperPacketsMode::Auto;
};

class NfbPlugin : public OutputPlugin {
public:
	/**
	 * @brief Construct NfbPlugin
	 *
	 * Constructs NfbPlugin and NfbQueue.
	 *
	 * @param[in] args string "arg1=value1,arg2=value2"
	 */
	explicit NfbPlugin(const std::string& params);

	~NfbPlugin();

	NfbPlugin(const NfbPlugin&) = delete;
	NfbPlugin(NfbPlugin&&) = delete;
	NfbPlugin& operator=(const NfbPlugin&) = delete;
	NfbPlugin& operator=(NfbPlugin&&) = delete;

	/**
	 * @brief Get queue count
	 *
	 * @return OutputQueue count
	 */
	size_t GetQueueCount() const noexcept override;

	/**
	 * @brief Get MTU of the nfb interface
	 */
	size_t GetMTU() const noexcept override;

	/**
	 * @brief Get pointer to ID-specific OutputQueue
	 *
	 * @param[in] queueID  Has to be in range of 0 - GetQueueCount()-1
	 *
	 * @return pointer to OutputQueue
	 */
	OutputQueue* GetQueue(uint16_t queueId) override;

	/**
	 * @brief Determines and configure the available offloads.
	 *
	 * @param offloads The requested offloads to configure.
	 * @return Configured offloads.
	 */
	Offloads ConfigureOffloads(const OffloadRequests& offloads) override;

private:
	void ParseArguments(const std::string& args);
	void ParsePluginConfiguration(const std::map<std::string, std::string>& argMap);

	void DeterminePacketSize();
	uint16_t GetInterfaceMTU();

	void OpenDevice();
	void ValidateQueueCount();

	NfbQueueBuilder CreateQueueBuilder();
	void SetReplicatorFirmareOptionsToQueueBuilder(NfbQueueBuilder& builder);
	bool ShouldUseSuperPacketsFeature(bool isSuperPacketFeatureEnabled);

	void PrepareNfbDevice();
	void SetFirmwareCapabilities();
	void CreateNfbQueues();
	void ResetTxMacCounters();
	uint64_t GetTxMacProcessedPacketsCount();

	void SetOffloadsToQueues(Offloads offloads);

	std::unique_ptr<nfb_device, decltype(&nfb_close)> _nfbDevice {nullptr, &nfb_close};
	std::vector<std::unique_ptr<NfbQueue>> _queues;

	NfbPluginConfig _pluginConfig;

	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("NfbPlugin");
};

} // namespace replay
