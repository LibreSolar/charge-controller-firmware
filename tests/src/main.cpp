/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#include "tests.h"

extern "C" {

void setUp(void)
{}

void tearDown(void)
{}

} /* extern "C" */

int main()
{
    int err = 0;

    err += daq_tests();
    err += bat_charger_tests();
    err += power_port_tests();
    err += half_bridge_tests();
    err += dcdc_tests();
    err += device_status_tests();
    err += load_tests();

#ifdef CONFIG_CUSTOM_TESTS
    err += custom_tests();
#endif

#ifdef CONFIG_ARCH_POSIX
    posix_exit(err);
#endif

    return 0;
}
