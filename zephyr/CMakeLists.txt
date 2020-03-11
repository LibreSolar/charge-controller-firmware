# SPDX-License-Identifier: Apache-2.0

set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/..)
set(DTS_ROOT ${CMAKE_SOURCE_DIR})

cmake_minimum_required(VERSION 3.13.1)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(libre_solar_charge_controller)

add_definitions(-D__ZEPHYR__ -DDEBUG_PRINT_FLAGS=7 -DCICADA_DEBUG)

include_directories(../src)

target_sources(app PRIVATE ../src/bat_charger.cpp)
target_sources(app PRIVATE ../src/data_objects.cpp)
target_sources(app PRIVATE ../src/daq.cpp)
target_sources(app PRIVATE ../src/daq_zephyr.c)
target_sources(app PRIVATE ../src/device_status.cpp)
target_sources(app PRIVATE ../src/dcdc.cpp)
target_sources(app PRIVATE ../src/eeprom.cpp)
target_sources(app PRIVATE ../src/half_bridge.cpp)
target_sources(app PRIVATE ../src/hardware.cpp)
target_sources(app PRIVATE ../src/helper.cpp)
target_sources(app PRIVATE ../src/leds.cpp)
target_sources(app PRIVATE ../src/load.cpp)
target_sources(app PRIVATE ../src/load_drv.cpp)
target_sources(app PRIVATE ../src/main_zephyr.cpp)
target_sources(app PRIVATE ../src/power_port.cpp)
target_sources(app PRIVATE ../src/pwm_switch.cpp)
target_sources(app PRIVATE ../src/setup.cpp)

target_sources(app PRIVATE ../src/ext/ext.cpp)
target_sources(app PRIVATE ../src/ext/modem.cpp)
target_sources(app PRIVATE ../src/ext/oled.cpp)
target_sources(app PRIVATE ../src/ext/serial.cpp)
target_sources(app PRIVATE ../src/ext/can.cpp)
target_sources(app PRIVATE ../src/ext/can_msg_queue.cpp)

# fix cpp static guard errors until PR #19754 gets merged
target_sources(app PRIVATE subsys/cpp/guards.c)

add_subdirectory(../lib/thingset build/thingset)
add_subdirectory(../lib/oled-display build/oled-display)
add_subdirectory(../lib/cicada-modem/cicada/platform/zephyr build/cicada)

zephyr_include_directories(../lib/paho-mqtt/MQTTClient/src)
zephyr_include_directories(../lib/paho-mqtt/MQTTPacket/src)
target_sources(app PRIVATE ../lib/paho-mqtt/MQTTPacket/src/MQTTPacket.c)
target_sources(app PRIVATE ../lib/paho-mqtt/MQTTPacket/src/MQTTDeserializePublish.c)
target_sources(app PRIVATE ../lib/paho-mqtt/MQTTPacket/src/MQTTSerializePublish.c)
target_sources(app PRIVATE ../lib/paho-mqtt/MQTTPacket/src/MQTTConnectClient.c)
