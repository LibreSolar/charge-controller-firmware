# SPDX-License-Identifier: Apache-2.0

option(USE_PLATFORMIO "Use PlatformIO build system" OFF)

set(BOARD mppt1210hus_0v4)
set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/..)

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(libre_solar_charge_controller)

add_definitions(-DMPPT_1210_HUS_0V4 -D__ZEPHYR__ -DDEBUG_PRINT_FLAGS=7)
include_directories(../src)
include_directories(../lib/Adafruit_GFX)

target_sources(app PRIVATE ../src/adc_dma.cpp)
target_sources(app PRIVATE ../src/bat_charger.cpp)
target_sources(app PRIVATE ../src/data_objects.cpp)
target_sources(app PRIVATE ../src/device_status.cpp)
target_sources(app PRIVATE ../src/dcdc.cpp)
target_sources(app PRIVATE ../src/eeprom.cpp)
target_sources(app PRIVATE ../src/half_bridge.cpp)
target_sources(app PRIVATE ../src/hardware.cpp)
target_sources(app PRIVATE ../src/leds.cpp)
target_sources(app PRIVATE ../src/load.cpp)
target_sources(app PRIVATE ../src/main_zephyr.cpp)
target_sources(app PRIVATE ../src/power_port.cpp)
target_sources(app PRIVATE ../src/pwm_switch.cpp)
target_sources(app PRIVATE ../src/ext/ext.cpp)
target_sources(app PRIVATE ../src/ext/oled.cpp)

target_sources(app PRIVATE ../lib/Adafruit_GFX/Adafruit_GFX.cpp)
target_sources(app PRIVATE ../lib/Adafruit_GFX/OledSSD1306.cpp)

if (NOT USE_PLATFORMIO)
    # manually include ThingSet library (you need to run platformio once to download library)
    include_directories(../.pio/libdeps/mppt-1210-hus-v0.4-zephyr/ThingSet/src)
    target_sources(app PRIVATE ../.pio/libdeps/mppt-1210-hus-v0.7/ThingSet/src/thingset.cpp)
    target_sources(app PRIVATE ../.pio/libdeps/mppt-1210-hus-v0.7/ThingSet/src/thingset_cbor.cpp)
    target_sources(app PRIVATE ../.pio/libdeps/mppt-1210-hus-v0.7/ThingSet/src/cbor.c)
    target_sources(app PRIVATE ../.pio/libdeps/mppt-1210-hus-v0.7/ThingSet/src/thingset_json.cpp)
    target_sources(app PRIVATE ../.pio/libdeps/mppt-1210-hus-v0.7/ThingSet/src/jsmn.c)
endif()
