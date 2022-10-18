# Find the native xxHash includes and library
#
# This module defines the following IMPORTED targets:
#
#  xxhash::xxhash      - The "xxhash" library, if found.
#
# This module will set the following variables in your project:
#
#  XXHASH_INCLUDE_DIRS - Where to find xxhash.h, etc.
#  XXHASH_LIBRARIES    - List of libraries when using xxhash.
#  XXHASH_FOUND        - True if xxhash found.

# Use pkg-config (if available) to get the library directories and then use
# these values as hints for find_path() and find_library() functions.
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(PC_XXHASH QUIET libxxhash)
endif()

find_path(
	XXHASH_INCLUDE_DIR xxhash.h
	HINTS ${PC_XXHASH_INCLUDEDIR} ${PC_XXHASH_INCLUDE_DIRS}
	PATH_SUFFIXES include
)

find_library(
	XXHASH_LIBRARY NAMES xxhash libxxhash
	HINTS ${PC_XXHASH_LIBDIR} ${PC_XXHASH_LIBRARY_DIRS}
	PATH_SUFFIXES lib lib64
)

if (PC_XXHASH_VERSION)
	# Version extracted from pkg-config
	set(XXHASH_VERSION_STRING ${PC_XXHASH_VERSION})
elseif (XXHASH_INCLUDE_DIR AND EXISTS "${XXHASH_INCLUDE_DIR}/xxhash.h")
	# Try to extract library version from a header file
	file(STRINGS "${XXHASH_INCLUDE_DIR}/xxhash.h" str_major
		REGEX "^#define[\t ]+XXH_VERSION_MAJOR[\t ]+.*")
	file(STRINGS "${XXHASH_INCLUDE_DIR}/xxhash.h" str_minor
		REGEX "^#define[\t ]+XXH_VERSION_MINOR[\t ]+.*")
	file(STRINGS "${XXHASH_INCLUDE_DIR}/xxhash.h" str_release
		REGEX "^#define[\t ]+XXH_VERSION_RELEASE[\t ]+.*")

	string(REGEX REPLACE "^#define[\t ]+XXH_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1"
		str_major "${str_major}")
	string(REGEX REPLACE "^#define[\t ]+XXH_VERSION_MINOR[\t ]+([0-9]+).*" "\\1"
		str_minor "${str_minor}")
	string(REGEX REPLACE "^#define[\t ]+XXH_VERSION_RELEASE[\t ]+([0-9]+).*" "\\1"
		str_release "${str_release}")

	set(XXHASH_VERSION_STRING "${str_major}.${str_minor}.${str_release}")

	unset(str_major)
	unset(str_minor)
	unset(str_release)
endif()

# Handle find_package() arguments (i.e. QUIETLY and REQUIRED) and set
# XXHASH_FOUND to TRUE if all listed variables are filled.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	xxHash
	REQUIRED_VARS XXHASH_LIBRARY XXHASH_INCLUDE_DIR
	VERSION_VAR XXHASH_VERSION_STRING
)

set(XXHASH_INCLUDE_DIRS ${XXHASH_INCLUDE_DIR})
set(XXHASH_LIBRARIES ${XXHASH_LIBRARY})
mark_as_advanced(XXHASH_INCLUDE_DIR XXHASH_LIBRARY)

if (XXHASH_FOUND)
	# Create imported library with all dependencies
	if (NOT TARGET xxhash::xxhash AND EXISTS "${XXHASH_LIBRARIES}")
		add_library(xxhash::xxhash UNKNOWN IMPORTED)
		set_target_properties(xxhash::xxhash PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${XXHASH_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${XXHASH_INCLUDE_DIRS}")
	endif()
endif()
