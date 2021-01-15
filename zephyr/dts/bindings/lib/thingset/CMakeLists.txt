# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(src)

target_sources(app PRIVATE src/thingset.cpp)
target_sources(app PRIVATE src/thingset_bin.cpp)
target_sources(app PRIVATE src/thingset_txt.cpp)
target_sources(app PRIVATE src/cbor.c)
target_sources(app PRIVATE src/jsmn.c)
