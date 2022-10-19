set(SOURCE_LIST
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
	list(APPEND SOURCE_LIST
		nfb.cpp
		nfb.hpp
	)
endif()

add_executable(ft-replay ${SOURCE_LIST})

target_link_libraries(ft-replay
	PRIVATE common
	-lpcap
	-lxxhash
	-lpthread
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
