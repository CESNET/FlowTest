/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"
#include "countDownLatch.hpp"
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

void ReplicationThread(
	Replicator replicator,
	std::unique_ptr<PacketQueue> packetQueue,
	CountDownLatch* workersLatch,
	size_t loopCount)
{
	size_t currentLoop = 0;

	workersLatch->ArriveAndWait();

	while (currentLoop != loopCount) {
		// TODO time wait
		// TODO synchronization
		replicator.SetReplicationLoopId(currentLoop);
		replicator.Replicate(packetQueue->begin(), packetQueue->end());
		currentLoop++;
	};
}

void ConfigureOutputQueueRateLimit(
	OutputQueue* outputQueue,
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

	outputQueue->SetRateLimiter(rateLimit);
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
	builder.SetTimeMultiplier(config.GetReplayTimeMultiplier());

	while ((rawPacket = packetProvider.Next())) {
		packetQueueProvider.InsertPacket(builder.Build(rawPacket));
	}

	CountDownLatch workersLatch(queueCount);

	for (size_t queueId = 0; queueId < queueCount; queueId++) {
		auto packetQueue = packetQueueProvider.GetPacketQueueById(queueId);
		auto packetQueueRatio = packetQueueProvider.GetPacketQueueRatioById(queueId);

		OutputQueue* outputQueue = outputPlugin->GetQueue(queueId);
		ConfigureOutputQueueRateLimit(outputQueue, packetQueueRatio, config.GetRateLimit());
		Replicator replicator(replicatorConfigParser.get(), outputQueue);

		std::thread worker(
			ReplicationThread,
			std::move(replicator),
			std::move(packetQueue),
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
		ReplicatorExecutor(config);
	} catch (const std::exception& ex) {
		logger->critical(ex.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
