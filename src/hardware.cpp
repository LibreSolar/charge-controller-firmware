/*
 * Copyright (c) The Libre Solar Project Contributors
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
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
    if ((FLASH->CR & FLASH_CR_OPTLOCK) != 0U) {
        /* Authorizes the Option Byte register programming */
        FLASH->OPTKEYR = FLASH_OPTKEY1;
        FLASH->OPTKEYR = FLASH_OPTKEY2;
    }

    // Set proper bits for booting the embedded bootloader (see table
    // 5 in section 2.6.1 in document RM0440)

    // nBOOT0: nBOOT0 option bit (equivalent to the BOOT0 pin)
    // nSWBOOT0: 0: BOOT0 taken from the option bit nBOOT0
    // nSWBOOT0: 1: BOOT0 taken from PB8/BOOT0 pin
    // nBOOT1: Together with the BOOT0 pin, this bit selects boot mode
    // from the Flash main memory, SRAM1 or the System memory.

    FLASH->OPTR &= ~(FLASH_OPTR_nSWBOOT0 | FLASH_OPTR_nBOOT0);
    FLASH->OPTR |= FLASH_OPTR_nBOOT1;

    // Save the current registers in flash, to be reloaded at reset
    FLASH->CR |= FLASH_CR_OPTSTRT;
    k_sleep(K_MSEC(20));

    // Reload the option registers from flash; should trigger a system
    // reset.
    FLASH->CR |= FLASH_CR_OBL_LAUNCH;

    // If OBL_LAUNCH did not reset (it should), we'll force it by
    // first locking back the flash registers and going for a reboot.
    FLASH->CR |= FLASH_CR_OPTLOCK;
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
