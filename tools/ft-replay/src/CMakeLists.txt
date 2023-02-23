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
	outputQueue.hpp
	raw.cpp
	raw.hpp
	socketDescriptor.cpp
	socketDescriptor.hpp
	replicator-core/configParser.hpp
	replicator-core/configParser.cpp
	replicator-core/configParserFactory.hpp
	replicator-core/configParserFactory.cpp
	replicator-core/configParserFactoryRegistrator.hpp
	replicator-core/yamlConfigParser.hpp
	replicator-core/yamlConfigParser.cpp
)

if (ENABLE_NFB)
	target_sources(ft-replay PRIVATE
		nfb.cpp
		nfb.hpp
	)

	target_link_libraries(ft-replay PRIVATE
		nfb::nfb
	)
endif()

target_link_libraries(ft-replay PRIVATE
	common
	pcap::pcap
	xxhash::xxhash
	Threads::Threads
	yaml-cpp::yaml-cpp
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
