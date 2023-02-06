/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"
#include "logger.h"
#include "outputPlugin.hpp"
#include "outputPluginFactory.hpp"
#include "outputQueue.hpp"
#include "packetBuilder.hpp"
#include "packetQueueProvider.hpp"
#include "rawPacketProvider.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace replay;

void ReplicationThread(const std::unique_ptr<PacketQueue> packetQueue, OutputQueue* outputQueue)
{
	(void) packetQueue;
	(void) outputQueue;
	// Replicator replicator(packetQueue, outputQueue);

	size_t maxBurstSize = outputQueue->GetMaxBurstSize();
	size_t burstSize = maxBurstSize;
	size_t packetsCount = packetQueue->size();
	size_t sendCount = 0;

	std::unique_ptr<PacketBuffer[]> burst = std::make_unique<PacketBuffer[]>(maxBurstSize);
	for (unsigned i = 0; i < maxBurstSize; i++) {
		burst[i]._len = 0;
	}

	for (unsigned r = 0; r < packetsCount; r += maxBurstSize) {
		if (r + maxBurstSize > packetsCount) {
			burstSize = packetsCount - r;
		}
		for (unsigned p = 0; p < burstSize; p++) {
			burst[p]._len = (*packetQueue)[p + r]->dataLen;
		}
		outputQueue->GetBurst(burst.get(), burstSize);
		for (unsigned p = 0; p < burstSize; p++) {
			std::byte* data = (*packetQueue)[p + r]->data.get();
			std::copy(data, data + burst[p]._len, burst[p]._data);
		}
		outputQueue->SendBurst(burst.get());
		sendCount += burstSize;
		burstSize = maxBurstSize;
	}
	std::cout << sendCount << " packets sent." << std::endl;
}

int main(int argc, char** argv)
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

	size_t queueCount = 1; // placeholder
	std::unique_ptr<OutputPlugin> outputPlugin;

	try {
		outputPlugin
			= OutputPluginFactory::Instance().Create(config.GetOutputPluginSpecification());
		RawPacketProvider packetProvider(config.GetInputPcapFile());
		PacketQueueProvider packetQueueProvider(queueCount);
		const RawPacket* rawPacket;

		PacketBuilder builder;
		builder.SetVlan(config.GetVlanID());
		builder.SetTimeMultiplier(config.GetReplayTimeMultiplier());

		while ((rawPacket = packetProvider.Next())) {
			packetQueueProvider.InsertPacket(builder.Build(rawPacket));
		}

		for (size_t queueId = 0; queueId < queueCount; queueId++) {
			auto packetQueue = packetQueueProvider.GetPacketQueueById(queueId);
			OutputQueue* outputQueue = outputPlugin->GetQueue(queueId);
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
