add_subdirectory(external)

# Create a common "object" library
set(COMMON_SRC
	src/logger.cpp
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
