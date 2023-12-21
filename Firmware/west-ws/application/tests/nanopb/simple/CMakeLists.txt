cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nanopb_app_tests)

message(WARNING "test/nanopb binary dir " ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/modules/application)

target_sources(app PRIVATE src/main.c)