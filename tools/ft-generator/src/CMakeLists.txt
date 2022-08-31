add_executable(ft-generator
	addressgenerators.cpp addressgenerators.h
	calendar.cpp calendar.h
	config.h config.cpp
	flow.cpp flow.h
	flowplanhelper.cpp flowplanhelper.h
	flowprofile.cpp flowprofile.h
	generator.cpp generator.h
	layer.h layer.cpp
	layers/ethernet.h layers/ethernet.cpp
	layers/ipv4.h layers/ipv4.cpp
	layers/ipv6.h layers/ipv6.cpp
	layers/payload.h layers/payload.cpp
	layers/tcp.h layers/tcp.cpp
	layers/udp.h layers/udp.cpp
	main.cpp
	packet.h packet.cpp
	packetflowspan.h packetflowspan.cpp
	pcapwriter.cpp pcapwriter.h
	trafficmeter.h trafficmeter.cpp
	valuegenerator.h valuegenerator.cpp
)

target_link_libraries(ft-generator
	PRIVATE
	pcapplusplus::common
	pcapplusplus::packet
	pcapplusplus::pcap
	common
)

install(TARGETS ft-generator DESTINATION ${INSTALL_DIR_BIN})
