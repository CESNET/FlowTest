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
)

target_link_libraries(ft-replay
	PRIVATE common
	-lpcap
	-lxxhash
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
