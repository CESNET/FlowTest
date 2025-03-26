add_subdirectory(external)

# Create a common "object" library
set(COMMON_SRC
	src/logger.cpp
	src/handlers.cpp
	src/timestamp.cpp
)

add_library(common OBJECT ${COMMON_SRC})

target_link_libraries(common
	PUBLIC
		spdlog::spdlog
)

target_include_directories(common
	PUBLIC
		include
		spdlog::spdlog
)

# To use common library (statically linked) in shared libraries (e.g. in python module ft-fast-analyzer)
# is needed to set position-independent code on.
set_property(TARGET common PROPERTY POSITION_INDEPENDENT_CODE ON)
