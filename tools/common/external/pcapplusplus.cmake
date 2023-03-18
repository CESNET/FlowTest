# PcapPlusPlus project
#
# The project is a multiplatform C++ network sniffing and packet parsing
# and crafting framework.
#
# The project consists of three libraries that can be independently
# added as dependency:
#  - pcapplusplus::common  (common code utilities used by the framework)
#  - pcapplusplus::packet  (parsing, creating and editing network packets)
#  - pcapplusplus::pcap    (intercepting and sending packets, etc.)

set(PPP_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/pcapplusplus")

# Prepare empty "include" directory as a CMake has issue with setting
# include directories of imported libraries built with `ExternalProject_Add`.
# https://gitlab.kitware.com/cmake/cmake/issues/15052
make_directory("${PPP_INSTALL_DIR}/include")

find_program(MAKE_EXE NAMES gmake nmake make)

ExternalProject_Add(pcapplusplus_build
	PREFIX pcapplusplus
	GIT_REPOSITORY "https://github.com/seladb/PcapPlusPlus"
	GIT_TAG "v22.11"
	GIT_SHALLOW ON

	UPDATE_COMMAND ""
	CONFIGURE_COMMAND
		"./configure-linux.sh"
		"--default"
		"--install-dir" "${PPP_INSTALL_DIR}"
	BUILD_IN_SOURCE ON
	BUILD_COMMAND
		"${MAKE_EXE}" "libs"

	LOG_DOWNLOAD ON
	LOG_CONFIGURE ON
	LOG_BUILD ON
	LOG_INSTALL ON
)

add_library(ppp_common STATIC IMPORTED GLOBAL)
add_library(ppp_packet STATIC IMPORTED GLOBAL)
add_library(ppp_pcap STATIC IMPORTED GLOBAL)

add_dependencies(ppp_common pcapplusplus_build)
add_dependencies(ppp_packet pcapplusplus_build)
add_dependencies(ppp_pcap pcapplusplus_build)

set_target_properties(ppp_common PROPERTIES
	IMPORTED_LOCATION "${PPP_INSTALL_DIR}/lib/libCommon++.a"
	INTERFACE_INCLUDE_DIRECTORIES "${PPP_INSTALL_DIR}/include"
)
set_target_properties(ppp_packet PROPERTIES
	IMPORTED_LOCATION "${PPP_INSTALL_DIR}/lib/libPacket++.a"
	INTERFACE_INCLUDE_DIRECTORIES "${PPP_INSTALL_DIR}/include"
)
set_target_properties(ppp_pcap PROPERTIES
	IMPORTED_LOCATION "${PPP_INSTALL_DIR}/lib/libPcap++.a"
	INTERFACE_INCLUDE_DIRECTORIES "${PPP_INSTALL_DIR}/include"
)

# By default, static ppp_cap library depends on libpcap so whoever
# wants to link the library, must also link libpcap.
target_link_libraries(ppp_pcap INTERFACE ${PCAP_LIBRARIES})

# Both packet and pcap framework libraries depends on the common library.
# Make sure the common library is included if any of them is used.
target_link_libraries(ppp_packet INTERFACE ppp_common)
target_link_libraries(ppp_pcap INTERFACE ppp_common)

# The framework depends on support for threads.
target_link_libraries(ppp_common INTERFACE ${CMAKE_THREAD_LIBS_INIT})

add_library(pcapplusplus::common ALIAS ppp_common)
add_library(pcapplusplus::packet ALIAS ppp_packet)
add_library(pcapplusplus::pcap ALIAS ppp_pcap)
