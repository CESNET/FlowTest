/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief NFB plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbPlugin.hpp"

#include "config.hpp"
#include "nfbReplicatorFirmware.hpp"
#include "outputPluginFactoryRegistrator.hpp"

extern "C" {
#include <netcope/txmac.h>
#include <nfb/ndp.h>
}

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace replay {

OutputPluginFactoryRegistrator<NfbPlugin> nfbPluginRegistration("nfb");

NfbPlugin::NfbPlugin(const std::string& params)
{
	try {
		ParseArguments(params);
		DeterminePacketSize();
		OpenDevice();
		ValidateQueueCount();
		ResetTxMacCounters();
		CreateNfbQueues();
	} catch (const std::exception& ex) {
		_logger->error(ex.what());
		throw std::runtime_error("NfbPlugin::NfbPlugin() has failed");
	}
}

void NfbPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	try {
		ParsePluginConfiguration(argMap);
	} catch (const std::invalid_argument& ia) {
		_logger->error(
			"Parameter \"queueCount\", \"burstSize\" or "
			"\"packetSize\" wrong format");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}

	if (_pluginConfig.deviceName.empty()) {
		_logger->error("Required parameter \"device\" missing/empty");
		throw std::invalid_argument("NfbPlugin::ParseArguments() has failed");
	}
}

void NfbPlugin::ParsePluginConfiguration(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "device") {
			_pluginConfig.deviceName = value;
		} else if (key == "queueCount") {
			_pluginConfig.queueCount = std::stoul(value);
		} else if (key == "burstSize") {
			_pluginConfig.maxBurstSize = std::stoul(value);
		} else if (key == "superPacket") {
			SuperPacketsMode superPacketsMode;
			if (value == "no") {
				superPacketsMode = SuperPacketsMode::Disable;
			} else if (value == "auto") {
				superPacketsMode = SuperPacketsMode::Auto;
			} else if (value == "yes") {
				superPacketsMode = SuperPacketsMode::Enable;
			} else {
				_logger->error("Unknown parameter value {}", value);
				throw std::runtime_error("NfbPlugin::ParsePluginConfiguration() has failed");
			}
			_pluginConfig.superPacketsMode = superPacketsMode;
		} else if (key == "packetSize") {
			_pluginConfig.packetSize = std::stoul(value);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("NfbPlugin::ParsePluginConfiguration() has failed");
		}
	}
}

void NfbPlugin::DeterminePacketSize()
{
	uint16_t mtu = GetInterfaceMTU();

	if (_pluginConfig.packetSize && _pluginConfig.packetSize > mtu) {
		_logger->error(
			"Packet size {} is bigger than interface MTU {}",
			_pluginConfig.packetSize,
			mtu);
		throw std::invalid_argument("NfbPlugin::DeterminePacketSize() has failed");
	}
	if (!_pluginConfig.packetSize) {
		_pluginConfig.packetSize = mtu;
		_logger->info(
			"Packet size not specified, using interface MTU {}",
			_pluginConfig.packetSize);
	}
}

void NfbPlugin::OpenDevice()
{
	_nfbDevice.reset(nfb_open(_pluginConfig.deviceName.c_str()));

	if (!_nfbDevice) {
		_logger->error("Unable to open NFB device \"{}\"", _pluginConfig.deviceName);
		throw std::runtime_error("NfbPlugin::OpenDevice() has failed");
	}
}

void NfbPlugin::ValidateQueueCount()
{
	int availableQueueCount = ndp_get_tx_queue_count(_nfbDevice.get());
	if (availableQueueCount <= 0) {
		_logger->error("ndp_get_tx_queue_count() has failed, returned: {}", availableQueueCount);
		throw std::runtime_error("NfbPlugin::ValidateQueueCount() has failed");
	}

	if (_pluginConfig.queueCount > static_cast<size_t>(availableQueueCount)) {
		_logger->error(
			"Requested queue count {} is greater than available queue count {}",
			_pluginConfig.queueCount,
			availableQueueCount);
		throw std::runtime_error("NfbPlugin::ValidateQueueCount() has failed");
	}

	if (_pluginConfig.queueCount == 0) {
		_pluginConfig.queueCount = static_cast<size_t>(availableQueueCount);
	}
}

void NfbPlugin::CreateNfbQueues()
{
	NfbQueueBuilder builder = CreateQueueBuilder();

	for (size_t queueId = 0; queueId < _pluginConfig.queueCount; queueId++) {
		auto nfbQueue = builder.Build(_nfbDevice.get(), queueId);
		_queues.emplace_back(std::move(nfbQueue));
	}
}

void NfbPlugin::ResetTxMacCounters()
{
	size_t txCount = nfb_comp_count(_nfbDevice.get(), COMP_NETCOPE_TXMAC);
	for (size_t txIndex = 0; txIndex < txCount; txIndex++) {
		std::unique_ptr<nc_txmac, decltype(&nc_txmac_close)> txmac {
			nc_txmac_open_index(_nfbDevice.get(), txIndex),
			&nc_txmac_close};
		if (!txmac) {
			_logger->error("nc_txmac_open_index() has failed");
			throw std::runtime_error("NfbPlugin::ResetTxMacCounters() has failed");
		}
		if (nc_txmac_reset_counters(txmac.get())) {
			_logger->error("nc_txmac_reset_counters() has failed");
			throw std::runtime_error("NfbPlugin::ResetTxMacCounters() has failed");
		}
	}
}

uint64_t NfbPlugin::GetTxMacProcessedPacketsCount()
{
	size_t txCount = nfb_comp_count(_nfbDevice.get(), COMP_NETCOPE_TXMAC);
	uint64_t processedPacketsCount = 0;
	for (size_t txIndex = 0; txIndex < txCount; txIndex++) {
		std::unique_ptr<nc_txmac, decltype(&nc_txmac_close)> txmac {
			nc_txmac_open_index(_nfbDevice.get(), txIndex),
			&nc_txmac_close};
		if (!txmac) {
			_logger->error("nc_txmac_open_index() has failed");
			throw std::runtime_error("NfbPlugin::GetTxMacProcessedPacketsCount() has failed");
		}
		struct nc_txmac_counters counters = {};
		if (nc_txmac_read_counters(txmac.get(), &counters)) {
			_logger->error("nc_txmac_read_counters() has failed");
			throw std::runtime_error("NfbPlugin::GetTxMacProcessedPacketsCount() has failed");
		}

		processedPacketsCount += counters.cnt_total;
	}
	return processedPacketsCount;
}

void NfbPlugin::SetReplicatorFirmareOptionsToQueueBuilder(NfbQueueBuilder& builder)
{
	NfbReplicatorFirmware replicatorFirmware(_nfbDevice.get());
	if (!replicatorFirmware.IsReplicatorFirmwareBooted()) {
		return;
	}

	builder.EnableReplicatorHeader();

	bool isSuperPacketsFeatureEnabled = replicatorFirmware.IsSuperPacketsFeatureSupported();
	bool useSuperPacketsFeature = ShouldUseSuperPacketsFeature(isSuperPacketsFeatureEnabled);
	if (useSuperPacketsFeature) {
		size_t superPacketLimit = replicatorFirmware.GetSuperPacketsLimit();
		builder.SetSuperPacketLimit(superPacketLimit);
	}
}

bool NfbPlugin::ShouldUseSuperPacketsFeature(bool isSuperPacketFeatureEnabled)
{
	SuperPacketsMode superPacketsMode = _pluginConfig.superPacketsMode;

	if (isSuperPacketFeatureEnabled) {
		if (superPacketsMode == SuperPacketsMode::Auto) {
			_logger->info("SuperPackets enabled");
			return true;
		} else if (superPacketsMode == SuperPacketsMode::Disable) {
			_logger->warn("SuperPackets are supported, but disabled");
			return false;
		}
		return true;
	}

	if (superPacketsMode == SuperPacketsMode::Auto) {
		_logger->info("SuperPackets disabled/unsupported");
	} else if (superPacketsMode == SuperPacketsMode::Enable) {
		_logger->warn("SuperPackets are not supported, but enabled");
	}
	return false;
}

NfbQueueBuilder NfbPlugin::CreateQueueBuilder()
{
	NfbQueueBuilder builder;
	builder.SetBurstSize(_pluginConfig.maxBurstSize);
	builder.SetPacketSize(_pluginConfig.packetSize);
	SetReplicatorFirmareOptionsToQueueBuilder(builder);
	return builder;
}

size_t NfbPlugin::GetQueueCount() const noexcept
{
	return _pluginConfig.queueCount;
}

uint16_t NfbPlugin::GetInterfaceMTU()
{
	std::string_view filename {"/sys/module/nfb/parameters/ndp_ctrl_buffer_size"};

	std::ifstream reader(filename.data());
	std::string line;

	if (!reader.is_open()) {
		_logger->error("Unable to open file {}, nfb module is probably not loaded", filename);
		throw std::runtime_error("NfbPlugin::GetInterfaceMTU() has failed");
	}

	std::getline(reader, line);

	std::istringstream stream(line.data());
	size_t fwBufferSize;

	stream >> fwBufferSize;

	constexpr size_t maxAlignement = 7;
	constexpr size_t metadataSize = sizeof(NfbReplicatorHeader) + maxAlignement;

	// TODO: this is a workaround for a bug in the firmware. Packet size is limited to max 14kB
	// bytes.
	constexpr size_t maxFwPacketSize = 14000 + metadataSize;

	fwBufferSize = std::min(fwBufferSize, maxFwPacketSize);

	return fwBufferSize - metadataSize;
}

size_t NfbPlugin::GetMTU() const noexcept
{
	return _pluginConfig.packetSize;
}

OutputQueue* NfbPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

Offloads NfbPlugin::ConfigureOffloads(const OffloadRequests& offloadRequests)
{
	NfbReplicatorFirmware replicatorFirmware(_nfbDevice.get());
	if (!replicatorFirmware.IsReplicatorFirmwareBooted()) {
		return Offloads {};
	}

	Offloads offloads = 0;
	Offloads fwSupportedOffloads = replicatorFirmware.GetOffloads();

	if (offloadRequests.checksumOffloads.checksumIPv4) {
		if (fwSupportedOffloads & Offload::CHECKSUM_IPV4) {
			offloads |= Offload::CHECKSUM_IPV4;
		}
	}

	if (offloadRequests.checksumOffloads.checksumTCP) {
		if (fwSupportedOffloads & Offload::CHECKSUM_TCP) {
			offloads |= Offload::CHECKSUM_TCP;
		}
	}

	if (offloadRequests.checksumOffloads.checksumUDP) {
		if (fwSupportedOffloads & Offload::CHECKSUM_UDP) {
			offloads |= Offload::CHECKSUM_UDP;
		}
	}

	if (offloadRequests.checksumOffloads.checksumICMPv6) {
		if (fwSupportedOffloads & Offload::CHECKSUM_ICMPV6) {
			offloads |= Offload::CHECKSUM_ICMPV6;
		}
	}

	auto rateLimit = offloadRequests.rateLimit;
	if (std::holds_alternative<Config::RateLimitPps>(rateLimit)) {
		if (fwSupportedOffloads & Offload::RATE_LIMIT_PACKETS) {
			replicatorFirmware.ConfigureLimiter(rateLimit);
			offloads |= Offload::RATE_LIMIT_PACKETS;
		}
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimit)) {
		if (fwSupportedOffloads & Offload::RATE_LIMIT_BYTES) {
			replicatorFirmware.ConfigureLimiter(rateLimit);
			offloads |= Offload::RATE_LIMIT_BYTES;
		}
	} else if (std::holds_alternative<Config::RateLimitTimeUnit>(rateLimit)) {
		if (fwSupportedOffloads & Offload::RATE_LIMIT_TIME) {
			replicatorFirmware.ConfigureLimiter(rateLimit);
			offloads |= Offload::RATE_LIMIT_TIME;
		}
	}

	SetOffloadsToQueues(offloads);

	return offloads;
}

void NfbPlugin::SetOffloadsToQueues(Offloads offloads)
{
	for (auto& queue : _queues) {
		queue->SetOffloads(offloads);
	}
}

NfbPlugin::~NfbPlugin()
{
	uint64_t sentPacketsCount = 0;
	for (size_t queueId = 0; queueId < _queues.size(); queueId++) {
		NfbQueue* outputQueue = _queues[queueId].get();
		sentPacketsCount += outputQueue->GetOutputQueueStats().transmittedPackets;
	}

	if (GetTxMacProcessedPacketsCount() == sentPacketsCount) {
		return;
	}

	_logger->info(
		"Waiting for txmac to process all packets. Sent: {}, processed: {}",
		sentPacketsCount,
		GetTxMacProcessedPacketsCount());

	while (GetTxMacProcessedPacketsCount() != sentPacketsCount) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

} // namespace replay
