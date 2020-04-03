/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware.h"

#include "mcu.h"
#include "board.h"
#include "load.h"
#include "half_bridge.h"
#include "leds.h"
#include "setup.h"

#if defined(STM32F0)
#define SYS_MEM_START       0x1FFFC800
#define SRAM_END            0x20003FFF  // 16k
#elif defined(STM32L0)
#define SYS_MEM_START       0x1FF00000
#define SRAM_END            0X20004FFF  // 20k
#define FLASH_LAST_PAGE     0x0802FF80  // start address of last page (192 kbyte cat 5 devices)
#endif

#define MAGIC_CODE_ADDR     (SRAM_END - 0xF)    // where the magic code is stored

#ifndef UNIT_TEST

#include <power/reboot.h>
#include <drivers/gpio.h>

void start_stm32_bootloader()
{
#ifdef DT_SWITCH_BOOT0_GPIOS_CONTROLLER
    // pin is connected to BOOT0 via resistor and capacitor
    struct device *dev = device_get_binding(DT_SWITCH_BOOT0_GPIOS_CONTROLLER);
    gpio_pin_configure(dev, DT_SWITCH_BOOT0_GPIOS_PIN,
        DT_SWITCH_BOOT0_GPIOS_FLAGS | GPIO_OUTPUT_ACTIVE);

    k_sleep(100);   // wait for capacitor at BOOT0 pin to charge up
    reset_device();
#elif defined (STM32F0)
    // place magic code at end of RAM and initiate reset
    *((uint32_t *)(MAGIC_CODE_ADDR)) = 0xDEADBEEF;
    reset_device();
#endif
}

void reset_device()
{
    sys_reboot(SYS_REBOOT_COLD);
}

#else

// dummy functions for unit tests
void start_stm32_bootloader() {}
void reset_device() {}

#endif /* UNIT_TEST */
