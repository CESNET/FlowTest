add_executable(ft-replay
	main.cpp
	config.hpp
	config.cpp
	packet.hpp
	packet.cpp
)

target_link_libraries(ft-replay
	-lpcap
	-lxxhash
)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
