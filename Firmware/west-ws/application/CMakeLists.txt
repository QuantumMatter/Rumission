zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
zephyr_include_directories(${NANOPB_INCLUDE_DIRS})
zephyr_include_directories(${CMAKE_BINARY_DIR})

message(WARNING "APP NANOPB DIRS " ${NANOPB_INCLUDE_DIRS})
message(WARNING "CURRENT BINARY DIR " ${CMAKE_CURRENT_BINARY_DIR})
message(WARNING "BINARY DIR " ${CMAKE_BINARY_DIR})

zephyr_include_directories(${NANOPB_INCLUDE_DIRS})
zephyr_include_directories(include)

add_subdirectory(Protos)

zephyr_include_directories(${CMAKE_BINARY_DIR})

add_subdirectory(drivers)
add_subdirectory(lib)