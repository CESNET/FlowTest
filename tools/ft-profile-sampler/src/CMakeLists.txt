add_executable(ft-profile-sampler
	biflow.cpp
	common.hpp
	evolution.cpp
	main.cpp
	metrics.cpp
	profile.cpp
)

target_compile_options(ft-profile-sampler PUBLIC -pthread)
target_link_libraries(ft-profile-sampler PRIVATE pthread common)
install(TARGETS ft-profile-sampler DESTINATION ${INSTALL_DIR_BIN})
