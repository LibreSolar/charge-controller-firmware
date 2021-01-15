# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(src)

target_sources(app PRIVATE src/adafruit_gfx.cpp)
target_sources(app PRIVATE src/oled_ssd1306.cpp)
