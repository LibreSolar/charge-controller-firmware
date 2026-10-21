# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# test-specific devicetree binding for the loop-back wiring
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(half_bridge_loopback)

target_sources(app PRIVATE src/capture.c src/main.c)
