/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"
#include "countDownLatch.hpp"
#include "freeMemoryChecker.hpp"
#include "logger.h"
#include "offloads.hpp"
#include "outputPlugin.hpp"
#include "outputPluginFactory.hpp"
#include "outputPluginStatsPrinter.hpp"
#include "outputQueue.hpp"
#include "packetBuilder.hpp"
#include "packetQueueProvider.hpp"
#include "rawPacketProvider.hpp"

#include "replicator-core/configParser.hpp"
#include "replicator-core/configParserFactory.hpp"
#include "replicator-core/replicator.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace replay;

void ReplicationThread(Replicator replicator, CountDownLatch* workersLatch, size_t loopCount)
{
	size_t currentLoop = 0;

	workersLatch->ArriveAndWait();

	while (currentLoop != loopCount) {
		replicator.Replicate(currentLoop);
		currentLoop++;
	};
}

Config::RateLimit CreateRateLimiterConfig(
	PacketQueueProvider::QueueDistribution queueDistribution,
	Config::RateLimit rateLimit)
{
	if (std::holds_alternative<Config::RateLimitPps>(rateLimit)) {
		auto& limit = std::get<Config::RateLimitPps>(rateLimit);
		const uint64_t packetsPerQueue = queueDistribution.packets * limit.value;
		const uint64_t max = std::max(packetsPerQueue, static_cast<uint64_t>(1));
		limit.value = max;
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimit)) {
		auto& limit = std::get<Config::RateLimitMbps>(rateLimit);
		const uint64_t bytesPerQueue = queueDistribution.bytes * limit.value;
		const uint64_t max = std::max(bytesPerQueue, static_cast<uint64_t>(1));
		limit.value = max;
	}

	return rateLimit;
}

Offloads ConfigureHwOffloads(
	const Config::RateLimit& rateLimit,
	OutputPlugin* outputPlugin,
	bool isHwOffloadsEnabled)
{
	if (!isHwOffloadsEnabled) {
		return 0;
	}

	OffloadRequests hwOffloadRequests {};
	hwOffloadRequests.checksumOffloads.checksumIPv4 = true;
	hwOffloadRequests.checksumOffloads.checksumTCP = true;
	hwOffloadRequests.checksumOffloads.checksumUDP = true;
	hwOffloadRequests.checksumOffloads.checksumICMPv6 = true;
	hwOffloadRequests.rateLimit = rateLimit;

	return outputPlugin->ConfigureOffloads(hwOffloadRequests);
}

void UpdateSwRateLimiterOffload(
	OffloadRequests& swOffloadRequests,
	Offloads configuredHwOffloads,
	const Config::RateLimit& rateLimit)
{
	if (std::holds_alternative<Config::RateLimitPps>(rateLimit)) {
		if (!(configuredHwOffloads & Offload::RATE_LIMIT_PACKETS)) {
			swOffloadRequests.rateLimit = rateLimit;
		}
	} else if (std::holds_alternative<Config::RateLimitMbps>(rateLimit)) {
		if (!(configuredHwOffloads & Offload::RATE_LIMIT_BYTES)) {
			swOffloadRequests.rateLimit = rateLimit;
		}
	} else if (std::holds_alternative<Config::RateLimitTimeUnit>(rateLimit)) {
		if (!(configuredHwOffloads & Offload::RATE_LIMIT_TIME)) {
			swOffloadRequests.rateLimit = rateLimit;
		}
	}
}

OffloadRequests
GetRequestedSwOffloads(const Config::RateLimit& rateLimit, Offloads configuredHwOffloads)
{
	OffloadRequests swOffloadRequests {};

	UpdateSwRateLimiterOffload(swOffloadRequests, configuredHwOffloads, rateLimit);

	if (!(configuredHwOffloads & Offload::CHECKSUM_IPV4)) {
		swOffloadRequests.checksumOffloads.checksumIPv4 = true;
	}

	if (!(configuredHwOffloads & Offload::CHECKSUM_TCP)) {
		swOffloadRequests.checksumOffloads.checksumTCP = true;
	}

	if (!(configuredHwOffloads & Offload::CHECKSUM_UDP)) {
		swOffloadRequests.checksumOffloads.checksumUDP = true;
	}

	if (!(configuredHwOffloads & Offload::CHECKSUM_ICMPV6)) {
		swOffloadRequests.checksumOffloads.checksumICMPv6 = true;
	}

	return swOffloadRequests;
}

void PrintConfiguredHwOffloads(const Offloads& configuredHwOffloads)
{
	auto logger = ft::LoggerGet("HwOffloads");
	if (!configuredHwOffloads) {
		logger->info("No HW offload enabled.");
		return;
	}

	std::string enabledOffloads;

	if (configuredHwOffloads & Offload::RATE_LIMIT_PACKETS
		|| configuredHwOffloads & Offload::RATE_LIMIT_BYTES
		|| configuredHwOffloads & Offload::RATE_LIMIT_TIME) {
		enabledOffloads += "rate limit, ";
	}

	if (configuredHwOffloads & Offload::CHECKSUM_IPV4) {
		enabledOffloads += "IPv4 checksum, ";
	}

	if (configuredHwOffloads & Offload::CHECKSUM_TCP) {
		enabledOffloads += "TCP checksum, ";
	}

	if (configuredHwOffloads & Offload::CHECKSUM_UDP) {
		enabledOffloads += "UDP checksum, ";
	}

	if (configuredHwOffloads & Offload::CHECKSUM_ICMPV6) {
		enabledOffloads += "ICMPv6 checksum";
	}

	// remove unwanted ", " at the end of the string
	std::size_t pos = enabledOffloads.find_last_of(", ");
	if (pos != std::string::npos && pos == enabledOffloads.length() - 1) {
		enabledOffloads.erase(pos, 2);
	}

	logger->info("Enabled HW offloads: " + enabledOffloads);
}

void ReplicatorExecutor(const Config& config)
{
	std::vector<std::thread> threads;
	std::unique_ptr<OutputPlugin> outputPlugin;
	std::unique_ptr<ConfigParser> replicatorConfigParser;
	outputPlugin = OutputPluginFactory::Instance().Create(config.GetOutputPluginSpecification());
	replicatorConfigParser = ConfigParserFactory::Instance().Create(config.GetReplicatorConfig());

	size_t queueCount = outputPlugin->GetQueueCount();
	auto configuredHwOffloads = ConfigureHwOffloads(
		config.GetRateLimit(),
		outputPlugin.get(),
		config.GetHwOffloadsSupport());
	PrintConfiguredHwOffloads(configuredHwOffloads);
	auto requestedSwOffloads = GetRequestedSwOffloads(config.GetRateLimit(), configuredHwOffloads);

	RawPacketProvider packetProvider(config.GetInputPcapFile());
	PacketQueueProvider packetQueueProvider(queueCount);
	const RawPacket* rawPacket;

	PacketBuilder builder;
	builder.SetVlan(config.GetVlanID());
	builder.SetSrcMac(config.GetSrcMacAddress());
	builder.SetDstMac(config.GetDstMacAddress());
	builder.SetTimeMultiplier(config.GetTimeMultiplier());
	builder.SetHwOffloads(configuredHwOffloads);

	while ((rawPacket = packetProvider.Next())) {
		packetQueueProvider.InsertPacket(builder.Build(rawPacket));
	}

	CountDownLatch workersLatch(queueCount);
	packetQueueProvider.PrintStats();

	auto loopTimeDuration = packetQueueProvider.GetPacketsTimeDuration();

	for (size_t queueId = 0; queueId < queueCount; queueId++) {
		auto packetQueue = packetQueueProvider.GetPacketQueueById(queueId);
		auto packetQueueRatio = packetQueueProvider.GetPacketQueueRatioById(queueId);
		auto rateLimiterConfig = CreateRateLimiterConfig(packetQueueRatio, config.GetRateLimit());
		UpdateSwRateLimiterOffload(requestedSwOffloads, configuredHwOffloads, rateLimiterConfig);

		OutputQueue* outputQueue = outputPlugin->GetQueue(queueId);
		Replicator replicator(std::move(packetQueue), outputQueue, loopTimeDuration);
		replicator.SetRequestedOffloads(requestedSwOffloads);
		replicator.SetReplicatorStrategy(replicatorConfigParser.get());

		std::thread worker(
			ReplicationThread,
			std::move(replicator),
			&workersLatch,
			config.GetLoopsCount());
		threads.emplace_back(std::move(worker));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	OutputPluginStatsPrinter outputPluginStatsPrinter(outputPlugin.get());
	outputPluginStatsPrinter.PrintStats();
}

void CheckSufficientMemory(const Config& config)
{
	if (!config.GetFreeRamCheck()) {
		return;
	}

	constexpr size_t freeMemoryOverheadPercentage = 5;

	try {
		FreeMemoryChecker freeMemoryChecker;
		bool isFreeMemory = freeMemoryChecker.IsFreeMemoryForFile(
			config.GetInputPcapFile(),
			freeMemoryOverheadPercentage);

		if (!isFreeMemory) {
			throw std::runtime_error("Not enough free RAM memory to process the pcap file");
		}
	} catch (std::exception& ex) {
		// Enrich with suppression option
		const std::string msg = ex.what();
		throw std::runtime_error(msg + " (suppress with --no-freeram-check)");
	}
}

int main(int argc, char** argv)
{
	Config config;

	ft::LoggerInit();
	auto logger = ft::LoggerGet("main");

	try {
		config.Parse(argc, argv);
	} catch (const std::invalid_argument& ex) {
		logger->critical(ex.what());
		config.PrintUsage();
		return EXIT_FAILURE;
	} catch (const std::exception& ex) {
		logger->critical(ex.what());
		return EXIT_FAILURE;
	}

	if (config.IsHelp()) {
		config.PrintUsage();
		return EXIT_SUCCESS;
	}

	try {
		CheckSufficientMemory(config);
		ReplicatorExecutor(config);
	} catch (const std::exception& ex) {
		logger->critical(ex.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
