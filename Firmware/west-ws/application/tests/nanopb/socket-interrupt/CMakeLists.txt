cmake_minimum_required(VERSION 3.20.0)

# add_definitions(-DPOSIX_ARCH_DEBUG_PRINTS)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nanopb_socket_tests)

target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/modules/application)

target_sources(app PRIVATE src/main.c)