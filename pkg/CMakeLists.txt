if (NOT RELEASE)
	string(TIMESTAMP RELEASE "%Y%m%d%H%M%S")
endif()

set(TGZ_FILE "${CMAKE_CURRENT_BINARY_DIR}/flowtest-${VERSION}-${RELEASE}.tar.gz")

add_subdirectory(tgz)
