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
)

if (ENABLE_NFB)
	target_sources(ft-replay PRIVATE
		nfb.cpp
		nfb.hpp
	)
endif()

target_link_libraries(ft-replay
	PRIVATE common
	-lpcap
	-lxxhash
	-lpthread
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
