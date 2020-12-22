/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware.h"

#include "mcu.h"
#include "load.h"
#include "half_bridge.h"
#include "leds.h"
#include "setup.h"

#ifndef UNIT_TEST

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), boot0))
#define BOOT0_GPIO DT_CHILD(DT_PATH(outputs), boot0)
#endif

#include <power/reboot.h>
#include <drivers/gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(hardware, CONFIG_HW_LOG_LEVEL);

void start_stm32_bootloader()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), boot0))

    // pin is connected to BOOT0 via resistor and capacitor
    const struct device *dev = device_get_binding(DT_GPIO_LABEL(BOOT0_GPIO, gpios));
    gpio_pin_configure(dev, DT_GPIO_PIN(BOOT0_GPIO, gpios),
        DT_GPIO_FLAGS(BOOT0_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);

    k_sleep(K_MSEC(100));   // wait for capacitor at BOOT0 pin to charge up
    reset_device();
#endif
}

void reset_device()
{
    sys_reboot(SYS_REBOOT_COLD);
}

void task_wdt_callback(int channel_id, void *user_data)
{
	printk("Task watchdog callback (channel: %d, thread: %s)\n",
		channel_id, k_thread_name_get((k_tid_t)user_data));

	printk("Resetting device...\n");

	sys_reboot(SYS_REBOOT_COLD);
}

#else

// dummy functions for unit tests
void start_stm32_bootloader() {}
void reset_device() {}
void task_wdt_callback(int channel_id, void *user_data) {}

#endif /* UNIT_TEST */
