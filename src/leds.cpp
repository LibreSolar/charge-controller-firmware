/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "leds.h"

#include "hardware.h"
#include "board.h"

static int led_states[NUM_LEDS];                // must be one of enum LedState
static int trigger_timeout[NUM_LEDS];           // seconds until trigger should end

static int blink_state = 1;                     // global state of all blinking LEDs

static bool charging = false;

#ifndef UNIT_TEST

#include <zephyr.h>
#include <drivers/gpio.h>

#define SLEEP_TIME_MS 	(1000/60/NUM_LEDS)		// 60 Hz

void leds_update_thread()
{
    int led_count = 0;
    int flicker_count = 0;
    bool flicker_state = 1;
    struct device *led_devs[NUM_LED_PINS];
    int wdt_channel = watchdog_register(1000);

    for (int pin = 0; pin < NUM_LED_PINS; pin++) {
        led_devs[pin] = device_get_binding(led_ports[pin]);
    }

    leds_init();

    while (1) {
        watchdog_feed(wdt_channel);

        // could be increased to value >NUM_LEDS to reduce on-time
        if (led_count >= NUM_LEDS) {
            led_count = 0;
        }

        if (flicker_count > 30) {
            flicker_count = 0;
            flicker_state = !flicker_state;
        }

        if (led_count < NUM_LEDS && (
                led_states[led_count] == LED_STATE_ON ||
                (led_states[led_count] == LED_STATE_FLICKER && flicker_state == 1) ||
                (led_states[led_count] == LED_STATE_BLINK && blink_state == 1)
        )) {
            for (int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
                switch (led_pin_setup[led_count][pin_number]) {
                    case PIN_HIGH:
                        gpio_pin_configure(led_devs[pin_number], led_pins[pin_number], GPIO_OUTPUT);
                        gpio_pin_set(led_devs[pin_number], led_pins[pin_number], 1);
                        break;
                    case PIN_LOW:
                        gpio_pin_configure(led_devs[pin_number], led_pins[pin_number], GPIO_OUTPUT);
                        gpio_pin_set(led_devs[pin_number], led_pins[pin_number], 0);
                        break;
                    case PIN_FLOAT:
                        gpio_pin_configure(led_devs[pin_number], led_pins[pin_number], GPIO_INPUT);
                        break;
                }
            }
        }
        else {
            // all pins floating
            for (int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
                gpio_pin_configure(led_devs[pin_number], led_pins[pin_number], GPIO_INPUT);
            }
        }

        led_count++;
        flicker_count++;

        k_sleep(SLEEP_TIME_MS);
    }
}

#endif  // UNIT_TEST

void leds_init(bool enabled)
{
    for (int led = 0; led < NUM_LEDS; led++) {
        if (enabled) {
            led_states[led] = LED_STATE_ON;
            trigger_timeout[led] = 0;           // switched off the first time the main loop is called
        }
        else {
            led_states[led] = LED_STATE_OFF;
            trigger_timeout[led] = -1;
        }
    }
}

void leds_set_charging(bool enabled)
{
    charging = enabled;
#ifdef LED_DCDC
    led_states[LED_DCDC] = LED_STATE_ON;
#endif
}

void leds_set(int led, bool enabled, int timeout)
{
    if (led >= NUM_LEDS || led < 0)
        return;

    if (enabled) {
        led_states[led] = LED_STATE_ON;
    }
    else {
        led_states[led] = LED_STATE_OFF;
    }

    trigger_timeout[led] = timeout;
}

void leds_on(int led, int timeout)
{
    leds_set(led, true, timeout);
}

void leds_off(int led)
{
    leds_set(led, false, -1);
}

void leds_blink(int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_BLINK;
        trigger_timeout[led] = timeout;
    }
}

void leds_flicker(int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_FLICKER;
        trigger_timeout[led] = timeout;
    }
}

void leds_update_1s()
{
    blink_state = !blink_state;

    for (int led = 0; led < NUM_LEDS; led++) {
        if (trigger_timeout[led] > 0) {
            trigger_timeout[led]--;
        }
        else if (trigger_timeout[led] == 0) {
            led_states[led] = LED_STATE_OFF;
        }
    }
}

void leds_toggle_error()
{
    for (int led = 0; led < NUM_LEDS; led++) {
        led_states[led] = (led % 2) ^ blink_state;
    }
    blink_state = !blink_state;
}

//----------------------------------------------------------------------------
void leds_update_soc(int soc, bool load_off_low_soc)
{
#ifndef LED_DCDC
    // if there is no dedicated LED for the DC/DC converter status, blink SOC
    // or power LED when charger is enabled
    int blink_chg = (charging) ? LED_STATE_BLINK : LED_STATE_ON;
#else
    int blink_chg = LED_STATE_ON;
#endif

#ifdef LED_PWR
    led_states[LED_PWR] = blink_chg;
    trigger_timeout[LED_PWR] = -1;
#elif defined(LED_SOC_3)  // 3-bar SOC gauge
    trigger_timeout[LED_SOC_1] = -1;
    trigger_timeout[LED_SOC_2] = -1;
    trigger_timeout[LED_SOC_3] = -1;
    if (soc > 80 && !load_off_low_soc) {
        led_states[LED_SOC_1] = blink_chg;
        led_states[LED_SOC_2] = LED_STATE_ON;
        led_states[LED_SOC_3] = LED_STATE_ON;
    }
    else if (soc > 20 && !load_off_low_soc) {
        led_states[LED_SOC_1] = LED_STATE_OFF;
        led_states[LED_SOC_2] = blink_chg;
        led_states[LED_SOC_3] = LED_STATE_ON;
    }
    else {
        led_states[LED_SOC_1] = LED_STATE_OFF;
        led_states[LED_SOC_2] = LED_STATE_OFF;
        led_states[LED_SOC_3] = blink_chg;
    }
#endif
}
