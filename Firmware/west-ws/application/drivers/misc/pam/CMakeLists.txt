list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

include_directories(${NANOPB_INCLUDE_DIRS})
zephyr_library()
zephyr_library_sources(pam.c)

# zephyr_library_sources(pam.c ${ZEPHYR_BASE}/../modules/lib/nanopb/pb_decode.c)