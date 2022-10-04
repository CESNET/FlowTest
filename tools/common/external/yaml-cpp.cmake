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

# Library does not compile with -Werror that we use in release builds
string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS_RELEASE_YAMLCPP "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE_YAMLCPP}")
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_INSTALL OFF)
FetchContent_MakeAvailable(yaml-cpp)
