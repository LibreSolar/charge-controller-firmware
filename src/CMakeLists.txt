# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

target_sources(app PRIVATE
        bat_charger.cpp
        data_nodes.cpp
        daq.cpp
        daq_driver.c
        device_status.cpp
        dcdc.cpp
        eeprom.cpp
        half_bridge.cpp
        hardware.cpp
        leds.cpp
        load.cpp
        load_driver.c
        main.cpp
        power_port.cpp
        pwm_switch_driver.c
        pwm_switch.cpp
        setup.cpp
)

add_subdirectory(ext)

if(${CONFIG_CUSTOM_DATA_NODES_FILE})
        target_sources(app PRIVATE data_nodes_custom.cpp)
endif()

# Using this option enables user-specific customization of the firmware. The
# custom subfolder should have its own CMakeLists.txt file.
if(${CONFIG_CUSTOMIZATION})
        add_subdirectory(custom build/custom)
endif()
