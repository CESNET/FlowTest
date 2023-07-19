# Find the GLPK includes and library
#
# This module defines the following IMPORTED targets:
#
#  glpk::glpk        - The "glpk" library, if found.
#
# This module will set the following variables in your project:
#
#  GLPK_INCLUDE_DIRS - where to find glpk.h, etc.
#  GLPK_LIBRARIES    - List of libraries when using glpk.
#  GLPK_FOUND        - True if glpk found.

include(FindPackageHandleStandardArgs)

find_path(GLPK_INCLUDE_DIR glpk.h)
find_library(GLPK_LIBRARY glpk)

if (GLPK_INCLUDE_DIR AND EXISTS "${GLPK_INCLUDE_DIR}/glpk.h")
	# Try to extract library version from a header file
	file(STRINGS "${GLPK_INCLUDE_DIR}/glpk.h" str_major
		REGEX "^#define[\t ]+GLP_MAJOR_VERSION[\t ]+.*")
	file(STRINGS "${GLPK_INCLUDE_DIR}/glpk.h" str_minor
		REGEX "^#define[\t ]+GLP_MINOR_VERSION[\t ]+.*")

	string(REGEX REPLACE "^#define[\t ]+GLP_MAJOR_VERSION[\t ]+([0-9]+).*" "\\1"
		str_major "${str_major}")
	string(REGEX REPLACE "^#define[\t ]+GLP_MINOR_VERSION[\t ]+([0-9]+).*" "\\1"
		str_minor "${str_minor}")

	set(GLPK_VERSION_STRING "${str_major}.${str_minor}")

	unset(str_major)
	unset(str_minor)
endif()

# Handle find_package() arguments (i.e. QUIETLY and REQUIRED) and set
# GLPK_FOUND to TRUE if all listed variables are filled.
find_package_handle_standard_args(GLPK
	REQUIRED_VARS GLPK_LIBRARY GLPK_INCLUDE_DIR
	VERSION_VAR GLPK_VERSION_STRING)

set(GLPK_INCLUDE_DIRS ${GLPK_INCLUDE_DIR})
set(GLPK_LIBRARIES ${GLPK_LIBRARY})
mark_as_advanced(GLPK_INCLUDE_DIR GLPK_LIBRARY)

if (GLPK_FOUND)
	# Create imported library with all dependencies
	if (NOT TARGET glpk::glpk AND EXISTS "${GLPK_LIBRARIES}")
		add_library(glpk::glpk UNKNOWN IMPORTED)
		set_target_properties(glpk::glpk PROPERTIES
			IMPORTED_LINK_INTERFACE_LANGUAGES "C"
			IMPORTED_LOCATION "${GLPK_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${GLPK_INCLUDE_DIRS}")
	endif()
endif()
