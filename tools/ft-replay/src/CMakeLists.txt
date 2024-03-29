# Allow to easily include files from nested directories using #include <...>
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(ft-replay
	main.cpp
	config.hpp
	config.cpp
	packet.hpp
	packet.cpp
	rawPacketProvider.hpp
	rawPacketProvider.cpp
	packetBuilder.hpp
	packetBuilder.cpp
	packetQueueProvider.hpp
	packetQueueProvider.cpp
	outputPlugin.hpp
	outputPlugin.cpp
	outputPluginFactory.hpp
	outputPluginFactory.cpp
	outputPluginFactoryRegistrator.hpp
	outputPluginStatsPrinter.hpp
	outputPluginStatsPrinter.cpp
	outputQueue.hpp
	outputQueue.cpp
	memoryMapping.hpp
	memoryMapping.cpp
	utils.cpp
	utils.hpp
	raw.cpp
	raw.hpp
	xdp.cpp
	xdp.hpp
	afPacket.cpp
	afPacket.hpp
	ethTool.cpp
	ethTool.hpp
	socketDescriptor.cpp
	socketDescriptor.hpp
	pcapFile.cpp
	pcapFile.hpp
	rateLimiter.hpp
	rateLimiter.cpp
	countDownLatch.hpp
	countDownLatch.cpp
	timeConverter.hpp
	timeDuration.cpp
	timeDuration.hpp
	freeMemoryChecker.hpp
	freeMemoryChecker.cpp
	checksumCalculator.cpp
	checksumCalculator.hpp
	replicator-core/configParser.hpp
	replicator-core/configParser.cpp
	replicator-core/configParserFactory.hpp
	replicator-core/configParserFactory.cpp
	replicator-core/configParserFactoryRegistrator.hpp
	replicator-core/yamlConfigParser.hpp
	replicator-core/yamlConfigParser.cpp
	replicator-core/ipAddress.hpp
	replicator-core/macAddress.hpp
	replicator-core/strategy.hpp
	replicator-core/strategy.cpp
	replicator-core/strategyFactory.hpp
	replicator-core/strategyFactory.cpp
	replicator-core/replicator.hpp
	replicator-core/replicator.cpp
	replicator-core/packetModifier.hpp
	replicator-core/packetModifier.cpp
	dissector/dissector.cpp
	dissector/layer.cpp
)

if (ENABLE_NFB)
	target_sources(ft-replay PRIVATE
		nfbDeviceTree.hpp
		nfbDeviceTree.cpp
		nfbCompatible.hpp
		nfbCompatible.cpp
		nfbFirmwareComponent.hpp
		nfbReplicatorCore.hpp
		nfbReplicatorCore.cpp
		nfbFrameUnpacker.hpp
		nfbFrameUnpacker.cpp
		nfbTimestampLimiter.hpp
		nfbTimestampLimiter.cpp
		nfbRateLimiter.hpp
		nfbRateLimiter.cpp
		nfbReplicatorFirmware.hpp
		nfbReplicatorFirmware.cpp
		nfbQueue.hpp
		nfbQueue.cpp
		nfbQueueBuilder.hpp
		nfbQueueBuilder.cpp
		nfbPlugin.hpp
		nfbPlugin.cpp
	)

	target_link_libraries(ft-replay PRIVATE
		nfb::nfb
		fdt::fdt
	)
endif()

target_link_libraries(ft-replay PRIVATE
	common
	pcap::pcap
	xdp::xdp
	xxhash::xxhash
	Threads::Threads
	yaml-cpp::yaml-cpp
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
