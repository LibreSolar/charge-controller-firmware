/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"

int main()
{
    daq_tests();
    bat_charger_tests();
    power_port_tests();
    half_brigde_tests();
    dcdc_tests();
    device_status_tests();
    load_tests();
}
