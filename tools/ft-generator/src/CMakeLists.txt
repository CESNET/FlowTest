add_executable(ft-generator
	addressgenerators.cpp addressgenerators.h
	calendar.cpp calendar.h
	config/commandlineargs.h config/commandlineargs.cpp
	config/common.h config/common.cpp
	config/config.h config/config.cpp
	config/ipv4.h config/ipv4.cpp
	config/ipv6.h config/ipv6.cpp
	config/mac.h config/mac.cpp
	flow.cpp flow.h
	flowplanhelper.cpp flowplanhelper.h
	flowprofile.cpp flowprofile.h
	generator.cpp generator.h
	generators/common.h generators/common.cpp
	generators/lfsr.h generators/lfsr.cpp
	generators/prefixedgenerator.h generators/prefixedgenerator.cpp
	layer.h layer.cpp
	layers/ethernet.h layers/ethernet.cpp
	layers/icmpecho.h layers/icmpecho.cpp
	layers/icmprandom.h layers/icmprandom.cpp
	layers/ipv4.h layers/ipv4.cpp
	layers/ipv6.h layers/ipv6.cpp
	layers/payload.h layers/payload.cpp
	layers/tcp.h layers/tcp.cpp
	layers/udp.h layers/udp.cpp
	main.cpp
	packet.h packet.cpp
	packetflowspan.h packetflowspan.cpp
	pcapwriter.cpp pcapwriter.h
	pcpppacket.h pcpppacket.cpp
	randomgenerator.h randomgenerator.cpp
	trafficmeter.h trafficmeter.cpp
	valuegenerator.h valuegenerator.cpp
)

target_link_libraries(ft-generator
	PRIVATE
	pcapplusplus::common
	pcapplusplus::packet
	pcapplusplus::pcap
	common
	yaml-cpp
)

install(TARGETS ft-generator DESTINATION ${INSTALL_DIR_BIN})
