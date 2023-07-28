# Yaml-cpp library
#
# yaml-cpp is a YAML parser and emitter in C++ matching the YAML 1.2 spec.
#
# "yaml-cpp" is exposed to be used as a dependency in other CMake targets
# example usage: target_link_libraries(my_target PRIVATE yaml-cpp)

include(FetchContent)

FetchContent_Declare(
	yaml-cpp
	GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
	GIT_TAG yaml-cpp-0.7.0
)

# Make sure that subproject accepts predefined build options without warnings.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Library does not compile with -Werror that we use in some builds
string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ")
string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ")
string(REPLACE "-Werror " " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_INSTALL OFF)
FetchContent_MakeAvailable(yaml-cpp)
