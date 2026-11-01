/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware.h"

#ifndef UNIT_TEST

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

void reset_device()
{
    sys_reboot(SYS_REBOOT_COLD);
}

void task_wdt_callback(int channel_id, void *user_data)
{
    printk("Task watchdog callback (channel: %d, thread: %s)\n", channel_id,
           k_thread_name_get((k_tid_t)user_data));

    printk("Resetting device...\n");

    sys_reboot(SYS_REBOOT_COLD);
}

#else

// dummy functions for unit tests
void reset_device()
{}
void task_wdt_callback(int channel_id, void *user_data)
{}

#endif /* UNIT_TEST */
