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

#if defined(CONFIG_SOC_SERIES_STM32F0X)
#define SYS_MEM_START       0x1FFFC800
#define SRAM_END            0x20003FFF  // 16k
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
#define SYS_MEM_START       0x1FF00000
#define SRAM_END            0X20004FFF  // 20k
#define FLASH_LAST_PAGE     0x0802FF80  // start address of last page (192 kbyte cat 5 devices)
#endif

#define MAGIC_CODE_ADDR     (SRAM_END - 0xF)    // where the magic code is stored

#ifndef UNIT_TEST

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), boot0))
#define BOOT0_GPIO DT_CHILD(DT_PATH(outputs), boot0)
#endif

#include <power/reboot.h>
#include <drivers/gpio.h>
#include <drivers/watchdog.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(hardware, CONFIG_LOG_DEFAULT_LEVEL);

#define MAX_SW_WDT_CHANNELS 5

struct sw_wdt_channel {
	int64_t check_in_time;  // most recent check-in/feed-in time of a thread for sw_watchdog
	uint32_t timeout;       // timeout of corresponding thread for sw_watchdog
};

static const struct device *wdt;
struct k_timer sw_wdt_timer;
struct sw_wdt_channel sw_wdt_channels[MAX_SW_WDT_CHANNELS];

int sw_wdt_channel_count = 0;
int hw_wdt_channel;

/*
 * Main software watchdog function called by kernel timer
 */
static void sw_watchdog(struct k_timer *timer_id);

void watchdog_init()
{
    wdt = device_get_binding(DT_LABEL(DT_NODELABEL(iwdg)));
    if (!wdt) {
        LOG_DBG("Cannot get IWDG device");
        return;
    }
}

int watchdog_register(uint32_t timeout_ms)
{
    if (sw_wdt_channel_count < MAX_SW_WDT_CHANNELS) {
        sw_wdt_channels[sw_wdt_channel_count].timeout = timeout_ms;
        return sw_wdt_channel_count++;
    }
    else {
        return -1;
    }
}

void watchdog_start()
{
    struct wdt_timeout_cfg wdt_config;
    wdt_config.flags = WDT_FLAG_RESET_SOC;
    wdt_config.window.min = 0U;
    wdt_config.window.max = 1000U;          // initialize with long timeout
    wdt_config.callback = NULL;             // STM32 does not support callbacks

    // look for smallest timeout in software watchdog channels
    for (int i = 0; i < sw_wdt_channel_count; i++) {
        if (sw_wdt_channels[i].timeout < wdt_config.window.max) {
            wdt_config.window.max = sw_wdt_channels[i].timeout;
        }
    }

    // start timer for software watchdog
    k_timer_init(&sw_wdt_timer, sw_watchdog, NULL);
    k_timer_start(&sw_wdt_timer, K_MSEC(10), K_MSEC(10));

    // finally start hardware watchdog
    wdt_setup(wdt, 0);
    hw_wdt_channel = wdt_install_timeout(wdt, &wdt_config);
}

void watchdog_feed(int thread_id)
{
    sw_wdt_channels[thread_id].check_in_time = k_uptime_get();
}

static void sw_watchdog(struct k_timer *timer_id)
{
    // feed also hardware watchdog
    wdt_feed(wdt, hw_wdt_channel);

    int64_t current_time = k_uptime_get();

    for (int i = 0; i < sw_wdt_channel_count; i++) {
        if ((current_time - sw_wdt_channels[i].check_in_time) > sw_wdt_channels[i].timeout) {
            LOG_ERR("Watchdog channel %d triggered!", i);
            reset_device();
        }
    }
}

void start_stm32_bootloader()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), boot0))

    // pin is connected to BOOT0 via resistor and capacitor
    const struct device *dev = device_get_binding(DT_GPIO_LABEL(BOOT0_GPIO, gpios));
    gpio_pin_configure(dev, DT_GPIO_PIN(BOOT0_GPIO, gpios),
        DT_GPIO_FLAGS(BOOT0_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);

    k_sleep(K_MSEC(100));   // wait for capacitor at BOOT0 pin to charge up
    reset_device();
#elif defined(CONFIG_SOC_SERIES_STM32F0X)
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
