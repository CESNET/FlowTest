/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"
#include "logger.h"
#include "outputPluginFactory.hpp"
#include "outputPlugin.hpp"
#include "outputQueue.hpp"
#include "packetQueueProvider.hpp"
#include "packetBuilder.hpp"
#include "rawPacketProvider.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace replay;

void ReplicationThread(
	const std::unique_ptr<PacketQueue> packetQueue,
	OutputQueue *outputQueue)
{
	(void) packetQueue;
	(void) outputQueue;
	//Replicator replicator(packetQueue, outputQueue);
}

int main(int argc, char **argv)
{
	Config config;
	std::vector<std::thread> threads;

	ft::LoggerInit();
	auto logger = ft::LoggerGet("main");

	try {
		config.Parse(argc, argv);
	} catch (const std::invalid_argument& ex) {
		logger->critical(ex.what());
		config.PrintUsage();
		return EXIT_FAILURE;
	}

	if (config.IsHelp()) {
		config.PrintUsage();
		return EXIT_SUCCESS;
	}

	size_t queueCount = 16; // placeholder
	std::unique_ptr<OutputPlugin> outputPlugin;

	try {
		outputPlugin = OutputPluginFactory::instance()
			.Create(config.GetOutputPluginSpecification());
		RawPacketProvider packetProvider(config.GetInputPcapFile());
		PacketQueueProvider packetQueueProvider(queueCount);
		const RawPacket *rawPacket;

		PacketBuilder builder;
		builder.SetVlan(config.GetVlanID());
		builder.SetTimeMultiplier(config.GetReplayTimeMultiplier());

		while ((rawPacket = packetProvider.Next())) {
			packetQueueProvider.InsertPacket(builder.Build(rawPacket));
		}

		for (size_t queueId = 0; queueId < queueCount; queueId++) {
			auto packetQueue = packetQueueProvider.GetPacketQueueById(queueId);
			OutputQueue *outputQueue = outputPlugin->GetQueue(queueId);
			std::thread thread(ReplicationThread, std::move(packetQueue), outputQueue);
			threads.emplace_back(std::move(thread));
		}
	} catch (const std::exception& ex) {
		logger->critical(ex.what());
		return EXIT_FAILURE;
	}

	for (auto& thread : threads) {
		thread.join();
	}

	return EXIT_SUCCESS;
}
