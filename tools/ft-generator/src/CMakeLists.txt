set(FT_GENERATOR_SOURCES
	calendar.cpp
	config/commandlineargs.cpp
	config/common.cpp
	config/config.cpp
	config/encapsulation.cpp
	config/ipv4.cpp
	config/ipv6.cpp
	config/mac.cpp
	config/packetsizeprobabilities.cpp
	flow.cpp
	flowplanhelper.cpp
	flowprofile.cpp
	generator.cpp
	generators/addressgenerators.cpp
	generators/ipv4addressgenerator.cpp
	generators/ipv6addressgenerator.cpp
	generators/lfsr.cpp
	generators/macaddressgenerator.cpp
	generators/multirangegenerator.cpp
	generators/prefixedgenerator.cpp
	layer.cpp
	layers/ethernet.cpp
	layers/icmpecho.cpp
	layers/icmprandom.cpp
	layers/icmpv6echo.cpp
	layers/icmpv6random.cpp
	layers/ipv4.cpp
	layers/ipv6.cpp
	layers/mpls.cpp
	layers/payload.cpp
	layers/tcp.cpp
	layers/udp.cpp
	layers/vlan.cpp
	packet.cpp
	packetflowspan.cpp
	pcapwriter.cpp
	pcppethlayer.cpp
	pcpppacket.cpp
	randomgenerator.cpp
	timeval.cpp
	trafficmeter.cpp
	packetsizegenerator.cpp
	packetsizegeneratorslow.cpp
	packetsizegeneratorfast.cpp
)

set(FT_GENERATOR_LIBS
	pcapplusplus::common
	pcapplusplus::packet
	pcapplusplus::pcap
	common
	yaml-cpp
	stdc++fs
	glpk::glpk
)

add_executable(ft-generator ${FT_GENERATOR_SOURCES} main.cpp)
target_link_libraries(ft-generator PRIVATE ${FT_GENERATOR_LIBS})

set(FT_GENERATOR_TEST_SOURCES
	tests/macaddressgenerator.cpp
	tests/lfsr.cpp
)

add_executable(ft-generator-test ${FT_GENERATOR_SOURCES} ${FT_GENERATOR_TEST_SOURCES} tests/main.cpp)
target_include_directories(ft-generator-test PRIVATE ${DOCTEST_INCLUDE_DIR})
target_link_libraries(ft-generator-test PRIVATE ${FT_GENERATOR_LIBS})

doctest_discover_tests(ft-generator-test
	ADD_LABELS 1
	TEST_PREFIX "ft-generator:")

install(TARGETS ft-generator DESTINATION ${INSTALL_DIR_BIN})
