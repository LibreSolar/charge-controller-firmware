/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unity.h>
#include <time.h>

void bat_charger_tests();

void daq_tests();

void power_port_tests();

void half_bridge_tests();

void dcdc_tests();

void device_status_tests();

void load_tests();

// activate this via build_flags in platformio.ini or custom.ini
#ifdef CUSTOM_TESTS
void custom_tests();
#endif
