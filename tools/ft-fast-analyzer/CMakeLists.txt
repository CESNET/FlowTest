add_library(fast-analyzer STATIC
	src/ipaddr.cpp
	src/flow.cpp
	src/flowtable.cpp
	src/statisticalmodel.cpp
)

# To use ft-analyzer library (statically linked) in shared libraries (e.g. in python module ft-fast-analyzer)
# is needed to set position-independent code on.
set_property(TARGET fast-analyzer PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(fast-analyzer PUBLIC src)

target_link_libraries(fast-analyzer PUBLIC pthread)
target_link_libraries(fast-analyzer PUBLIC common)
