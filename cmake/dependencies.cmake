# Project dependencies
find_package(PkgConfig REQUIRED)

find_package(PCAP REQUIRED)
find_package(xxHash REQUIRED)
find_package(Threads REQUIRED)
find_package(GLPK REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(numa REQUIRED IMPORTED_TARGET numa)

# DPDK framework
if (ENABLE_DPDK)
	pkg_check_modules(libdpdk REQUIRED IMPORTED_TARGET libdpdk)
endif()

# NFB framework for CESNET FPGA cards
if (ENABLE_NFB)
	find_package(NFB REQUIRED)
	find_package(FDT REQUIRED)
endif()

# XDP (eXpress Data Path) dependencies
# AF_XDP socket (XSK) functions were originally implemented within libbpf
# library. Later they were extracted into a standalone library called libxdp.
# The code below creates xdp::xdp pseudo-library that wraps both dependencies.
# If libxdp library is present in the system, FT_XDP_LIBXDP definition is
# specified at compile time and the user should use the header files of the
# new library.
pkg_check_modules(libbpf REQUIRED IMPORTED_TARGET libbpf)
pkg_check_modules(libxdp IMPORTED_TARGET libxdp)

add_library(xdp INTERFACE)
add_library(xdp::xdp ALIAS xdp)

if (libxdp_FOUND)
	# Both libraries are available
	target_link_libraries(xdp INTERFACE PkgConfig::libbpf PkgConfig::libxdp)
	target_compile_definitions(xdp INTERFACE FT_XDP_LIBXDP)
else()
	# Only libbpf is available
	if (libbpf_VERSION VERSION_GREATER_EQUAL "0.7.0")
		# Since libbpf >= 0.7.0 support for AF_XDP has been deprecated
		message(FATAL_ERROR "libxdp-devel (or libbpf-devel < 0.7.0) not found")
	endif()

	# Rely solely on libbpf for XSK functionality
	target_link_libraries(xdp INTERFACE PkgConfig::libbpf)
endif()
