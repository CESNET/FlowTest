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

void ReplicatorExecutor(const Config& config)
{
	std::vector<std::thread> threads;
	std::unique_ptr<OutputPlugin> outputPlugin;
	std::unique_ptr<ConfigParser> replicatorConfigParser;
	outputPlugin = OutputPluginFactory::Instance().Create(config.GetOutputPluginSpecification());
	replicatorConfigParser = ConfigParserFactory::Instance().Create(config.GetReplicatorConfig());

	size_t queueCount = outputPlugin->GetQueueCount();
	RawPacketProvider packetProvider(config.GetInputPcapFile());
	PacketQueueProvider packetQueueProvider(queueCount);
	const RawPacket* rawPacket;

	PacketBuilder builder;
	builder.SetVlan(config.GetVlanID());

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

		OutputQueue* outputQueue = outputPlugin->GetQueue(queueId);
		Replicator replicator(std::move(packetQueue), outputQueue, loopTimeDuration);
		replicator.SetRateLimiter(rateLimiterConfig);
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

	FreeMemoryChecker freeMemoryChecker;
	bool isFreeMemory = freeMemoryChecker.IsFreeMemoryForFile(
		config.GetInputPcapFile(),
		freeMemoryOverheadPercentage);

	if (!isFreeMemory) {
		throw std::runtime_error(
			"Not enough free RAM memory to process the pcap file (supress with "
			"--no-freeram-check).");
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
