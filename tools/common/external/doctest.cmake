# doctest - a C++ testing framework
#
# To use, add DOCTEST_INCLUDE_DIR to your target includes:
#    target_include_directories(tests PRIVATE ${DOCTEST_INCLUDE_DIR})

find_package(Git REQUIRED)

ExternalProject_Add(
	doctest
	PREFIX ${CMAKE_BINARY_DIR}/doctest

	GIT_REPOSITORY https://github.com/doctest/doctest.git
	GIT_TAG "v2.4.11"
	GIT_SHALLOW ON

	UPDATE_COMMAND ""
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND ""
	LOG_DOWNLOAD ON
)

# Expose required variable (DOCTEST_INCLUDE_DIR) to parent scope
ExternalProject_Get_Property(doctest source_dir)
set(DOCTEST_INCLUDE_DIR ${source_dir}/doctest CACHE INTERNAL "Path to include folder for doctest")
