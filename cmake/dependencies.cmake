# Project dependencies
find_package(PCAP REQUIRED)
find_package(xxHash REQUIRED)
find_package(Threads REQUIRED)

# NFB framework for CESNET FPGA cards
if (ENABLE_NFB)
	find_package(NFB REQUIRED)
endif()
