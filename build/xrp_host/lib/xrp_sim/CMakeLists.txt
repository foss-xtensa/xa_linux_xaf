SET(CMAKE_C_FLAGS "-W -Wall -DHAVE_ATOMIC")

set(XRP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/xrp_threaded_queue.c
             ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/xrp_host_common.c
             ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/standalone/xrp_host.c
             ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/standalone/xrp_linux.c
             ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-kernel/xrp_alloc.c)

include_directories(${CMAKE_SOURCE_DIR}/include/xrp
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/thread-pthread
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-host/standalone
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-common
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xrp_src/xrp-kernel)

add_library(xrp-linux-sim ${XRP_SRCS})

install(TARGETS xrp-linux-sim 
        ARCHIVE DESTINATION host/lib)
