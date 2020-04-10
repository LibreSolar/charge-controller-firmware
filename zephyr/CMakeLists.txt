# SPDX-License-Identifier: Apache-2.0

set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/..)
set(DTS_ROOT ${CMAKE_SOURCE_DIR})

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(libre_solar_charge_controller)

add_definitions(-D__ZEPHYR__ -DDEBUG_PRINT_FLAGS=7)

include_directories(../src)

target_sources(app PRIVATE
        ../src/bat_charger.cpp
        ../src/data_objects.cpp
        ../src/daq.cpp
        ../src/daq_driver.c
        ../src/device_status.cpp
        ../src/dcdc.cpp
        ../src/eeprom.cpp
        ../src/half_bridge.cpp
        ../src/hardware.cpp
        ../src/helper.cpp
        ../src/leds.cpp
        ../src/load.cpp
        ../src/load_driver.c
        ../src/main.cpp
        ../src/power_port.cpp
        ../src/pwm_switch_driver.c
        ../src/pwm_switch.cpp
        ../src/setup.cpp
        ../src/ext/ext.cpp
        ../src/ext/oled.cpp
        ../src/ext/serial.cpp
        ../src/ext/can.cpp
        ../src/ext/can_msg_queue.cpp
)

add_subdirectory(../lib/thingset build/thingset)
add_subdirectory(../lib/oled-display build/oled-display)
#add_subdirectory_ifdef(CONFIG_EXT_OLED_DISPLAY ../lib/oled-display)
