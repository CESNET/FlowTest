include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

# Use FindPython (find_package) in CMakeLists before including this file
set(PYBIND11_FINDPYTHON OFF)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
	pybind11
	URL https://github.com/pybind/pybind11/archive/v2.13.6.tar.gz
	URL_HASH SHA256=e08cb87f4773da97fa7b5f035de8763abc656d87d5773e62f6da0587d1f0ec20
	)

FetchContent_GetProperties(pybind11)

if(NOT pybind11_POPULATED)
	FetchContent_Populate(pybind11)
	add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
endif()
