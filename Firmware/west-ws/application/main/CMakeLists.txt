# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# set(BOARD esp32_devkitc_wrover)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

message(WARNING "app/main binary dir " ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/modules/application)

# list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
# list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_NANOPB_MODULE_DIR}/extra)
# include(nanopb)

# # zephyr_nanopb_sources(${CMAKE_CURRENT_SOURCE_DIR}/../Protos/simple.proto)
# # zephyr_nanopb_sources(app src/simple.proto)

# find_package(Nanopb REQUIRED)
# set(nanopb_BUILD_RUNTIME ON )
# set(NANOPB_GENERATE_CPP_STANDALONE OFF)
# include_directories(${NANOPB_INCLUDE_DIRS})
# nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH . 
#                         ../Protos/src/sample.proto
#                         ../Protos/src/rpc.proto)
# target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
# target_sources(app PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})

target_sources(app PRIVATE src/main.c)