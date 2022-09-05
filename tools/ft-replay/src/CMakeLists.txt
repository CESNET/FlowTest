add_executable(ft-replay
	main.cpp
	config.hpp
	config.cpp
)

target_link_libraries(ft-replay)

install(TARGETS ft-replay DESTINATION ${INSTALL_DIR_BIN})
