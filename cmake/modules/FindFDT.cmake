# Find the fdt includes and library
#
# This module defines the following IMPORTED targets:
#
#  fdt::fdt          - The "fdt" library, if found.
#
# This module will set the following variables in your project:
#
#  FDT_INCLUDE_DIRS - where to find <fdt.h>, etc.
#  FDT_LIBRARIES    - List of libraries when using fdt.
#  FDT_FOUND        - True if the fdt has been found.

# Use pkg-config (if available) to get the library directories and then use
# these values as hints for find_path() and find_library() functions.
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(PC_FDT QUIET libfdt)
endif()

find_path(
	FDT_INCLUDE_DIR fdt.h
	HINTS ${PC_FDT_INCLUDEDIR} ${PC_FDT_INCLUDE_DIRS}
	PATH_SUFFIXES include
)

find_library(
	FDT_LIBRARY NAMES fdt libfdt
	HINTS ${PC_FDT_LIBDIR} ${PC_FDT_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

if (PC_FDT_VERSION)
	# Version extracted from pkg-config
	set(FDT_VERSION_STRING ${PC_FDT_VERSION})
endif()

# Handle find_package() arguments (i.e. QUIETLY and REQUIRED) and set
# FDT_FOUND to TRUE if all listed variables are filled.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	FDT
	REQUIRED_VARS FDT_LIBRARY FDT_INCLUDE_DIR
	VERSION_VAR FDT_VERSION_STRING
)

set(FDT_INCLUDE_DIRS ${FDT_INCLUDE_DIR})
set(FDT_LIBRARIES ${FDT_LIBRARY})
mark_as_advanced(FDT_INCLUDE_DIR FDT_LIBRARY)

if (FDT_FOUND)
	# Create imported library with all dependencies
	if (NOT TARGET fdt::fdt AND EXISTS "${FDT_LIBRARIES}")
		add_library(fdt::fdt UNKNOWN IMPORTED)
		set_target_properties(fdt::fdt PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${FDT_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${FDT_INCLUDE_DIRS}")
	endif()
endif()
