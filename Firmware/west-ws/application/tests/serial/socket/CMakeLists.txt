cmake_minimum_required(VERSION 3.20.0)

# add_definitions(-DPOSIX_ARCH_DEBUG_PRINTS)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(uart_socket_test)

target_sources(app PRIVATE src/main.c)