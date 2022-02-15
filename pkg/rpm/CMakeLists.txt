find_program(RPMBUILD_EXECUTABLE NAMES rpmbuild REQUIRED)

set(RPMBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/rpmbuild")

file(MAKE_DIRECTORY
	"${RPMBUILD_DIR}/BUILD"
	"${RPMBUILD_DIR}/RPMS"
	"${RPMBUILD_DIR}/SOURCES"
	"${RPMBUILD_DIR}/SPECS"
	"${RPMBUILD_DIR}/SRPMS"
)

set(SOURCE_TGZ "flowtest-${VERSION}.tar.gz")
set(SPEC_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/flowtest.spec.in")
set(SPEC_FILE "${RPMBUILD_DIR}/SPECS/flowtest.spec")

configure_file("${SPEC_FILE_IN}" "${SPEC_FILE}" @ONLY)

add_custom_target(rpm
	COMMENT "Generating SRPM and RPM packages..."
	WORKING_DIRECTORY "${RPMBUILD_DIR}"

	COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${TGZ_FILE}" "SOURCES/${SOURCE_TGZ}"
	COMMAND "${RPMBUILD_EXECUTABLE}" "-ba" ${SPEC_FILE} "--define" "_topdir ${RPMBUILD_DIR}/"
)

add_dependencies(rpm
	tgz
)

add_custom_command(TARGET rpm POST_BUILD
	COMMENT "SRPM and RPM packages are located in ${RPMBUILD_DIR}/"
	COMMAND ;
)
