# Find the native PCAP includes and library
#
# This module defines the following IMPORTED targets:
#
#  pcap::pcap        - The "libpcap" library, if found.
#
# This module will set the following variables in your project:
#
#  PCAP_INCLUDE_DIRS - where to find pcap.h, etc.
#  PCAP_LIBRARIES    - List of libraries when using pcap.
#  PCAP_FOUND        - True if pcap found.

# Use pkg-config (if available) to get the library directories and then use
# these values as hints for find_path() and find_library() functions.
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(PC_PCAP QUIET libpcap)
endif()

find_path(
	PCAP_INCLUDE_DIR pcap/pcap.h
	HINTS ${PC_PCAP_INCLUDEDIR} ${PC_PCAP_INCLUDE_DIRS}
	PATH_SUFFIXES include
)

find_library(
	PCAP_LIBRARY NAMES pcap libpcap
	HINTS ${PC_PCAP_LIBDIR} ${PC_PCAP_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

if (PC_PCAP_VERSION)
	# Version extracted from pkg-config
	set(PCAP_VERSION_STRING ${PC_PCAP_VERSION})
elseif(PCAP_INCLUDE_DIR AND PCAP_LIBRARY)
	# Try to get the version of installed library
	try_run(
		PCAP_RES_RUN PCAP_RES_COMP
		${CMAKE_CURRENT_BINARY_DIR}/try_run/pcap_version_test/
		${PROJECT_SOURCE_DIR}/cmake/modules/try_run/pcap_version.c
		CMAKE_FLAGS
			-DLINK_LIBRARIES=${PCAP_LIBRARY}
			-DINCLUDE_DIRECTORIES=${PCAP_INCLUDE_DIR}
		RUN_OUTPUT_VARIABLE PCAP_VERSION_VAR
	)

	if (PCAP_RES_COMP AND PCAP_RES_RUN EQUAL 0)
		# Successfully compiled and executed with return code 0
		# Expected format e.g. "libpcap version 1.9.1 (with TPACKET_V3)"
		string(REGEX REPLACE "^[^0-9]*([0-9.]+).*" "\\1"
			PCAP_VERSION_STRING "${PCAP_VERSION_VAR}")
	endif()
endif()

# Handle find_package() arguments (i.e. QUIETLY and REQUIRED) and set
# PCAP_FOUND to TRUE if all listed variables are filled.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	PCAP
	REQUIRED_VARS PCAP_LIBRARY PCAP_INCLUDE_DIR
	VERSION_VAR PCAP_VERSION_STRING
)

set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
set(PCAP_LIBRARIES ${PCAP_LIBRARY})
mark_as_advanced(PCAP_INCLUDE_DIR PCAP_LIBRARY)

if (PCAP_FOUND)
	# Create imported library with all dependencies
	if (NOT TARGET pcap::pcap AND EXISTS "${PCAP_LIBRARIES}")
		add_library(pcap::pcap UNKNOWN IMPORTED)
		set_target_properties(pcap::pcap PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${PCAP_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${PCAP_INCLUDE_DIRS}")
	endif()
endif()
