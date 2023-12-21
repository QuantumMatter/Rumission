list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
include_directories(${NANOPB_INCLUDE_DIRS})

zephyr_library_named(i2c_rpc_dts_driver)
zephyr_library_sources(i2c_rpc.c)

add_dependencies(i2c_rpc_dts_driver rumission_protos)
