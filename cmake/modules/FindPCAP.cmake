# Find the native PCAP includes and library
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

# Handle find_package() arguments (i.e. QUIETLY and REQUIRED) and set
# PCAP_FOUND to TRUE if all listed variables are filled.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	PCAP
	REQUIRED_VARS PCAP_LIBRARY PCAP_INCLUDE_DIR
)

set(PCAP_INCLUDE_DIRS ${PCAP_INCLUDE_DIR})
set(PCAP_LIBRARIES ${PCAP_LIBRARY})
mark_as_advanced(PCAP_INCLUDE_DIR PCAP_LIBRARY)
