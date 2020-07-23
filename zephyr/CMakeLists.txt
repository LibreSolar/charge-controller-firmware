# SPDX-License-Identifier: Apache-2.0

set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/..)
set(DTS_ROOT ${CMAKE_SOURCE_DIR})

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(libre_solar_charge_controller)

add_subdirectory(../src build/src)

add_subdirectory(../lib/thingset build/thingset)
add_subdirectory(../lib/oled-display build/oled-display)
