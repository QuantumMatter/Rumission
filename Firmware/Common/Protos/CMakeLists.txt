# list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
# include(nanopb)

# # zephyr_library()
# # zephyr_nanopb_sources(app src/simple.proto)

# set(NANOPB_GENERATE_CPP_STANDALONE OFF)
# nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH Protos src/simple.proto)
# target_sources(app PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})

zephyr_library_named(rumission_protos)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_NANOPB_MODULE_DIR}/extra)
include(nanopb)

add_definitions(-DPB_ENABLE_MALLOC)

zephyr_include_directories(${NANOPB_INCLUDE_DIRS})

# zephyr_nanopb_sources(${CMAKE_CURRENT_SOURCE_DIR}/../Protos/simple.proto)
zephyr_nanopb_sources(${ZEPHYR_CURRENT_LIBRARY} sample.proto rpc.proto)

add_subdirectory(v1)

# find_package(Nanopb REQUIRED)
# set(nanopb_BUILD_RUNTIME ON )
# set(NANOPB_GENERATE_CPP_STANDALONE OFF)
# include_directories(${NANOPB_INCLUDE_DIRS})
# nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH . 
#                         ../Protos/src/sample.proto
#                         ../Protos/src/rpc.proto)
# target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
# target_sources(app PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})