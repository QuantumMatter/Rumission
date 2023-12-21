list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

include_directories(${NANOPB_INCLUDE_DIRS})

zephyr_library_named(rumission_rpc)
zephyr_library_sources(rpc.c)

add_dependencies(rumission_rpc rumission_protos)