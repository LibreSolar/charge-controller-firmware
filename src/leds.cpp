/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "leds.h"

#include <zephyr.h>
#ifndef UNIT_TEST
#include <drivers/gpio.h>
#endif

#include "hardware.h"

#define SLEEP_TIME_MS 	(1000/60/NUM_LEDS)		// 60 Hz

/*
 * The LED setup is derived from the devicetree configuration in board.dts file
 *
 * Example devicetree fragment:
 *
 * leds {
 *     compatible = "charlieplexing-leds";
 *     gpios = <&gpioa 1 GPIO_ACTIVE_HIGH>,
 *             <&gpiob 2 GPIO_ACTIVE_HIGH>,
 *             <&gpioc 3 GPIO_ACTIVE_HIGH>;
 *     soc_1 {
 *         states = <1 0 2>;
 *     };
 *     soc_2 {
 *         states = <0 1 2>;
 *     };
 *     soc_3 {
 *         states = <1 2 0>;
 *     };
 *     load {
 *         states = <2 0 1>;
 *     };
 * };
 */

/*
 * Number of LED pins = length of "gpios" array in "leds" devicetree node
 */
#define NUM_LED_PINS DT_PROP_LEN(DT_PATH(leds), gpios)

/*
 * LED pin states to switch a specific LED on
 *
 * 0 = low, 1 = high, 2 = floating (see enum PinState in leds.h)
 *
 * Above example board.dts expands to:
 *
 * static const int led_pin_states[4][3] {
 *     {1, 0, 2},
 *     {0, 1, 2},
 *     {1, 2, 0},
 *     {2, 0, 1},
 * };
 */
#define LED_STATES(node_id) DT_PROP(node_id, states),
static const int led_pin_states[NUM_LEDS][NUM_LED_PINS] = {
    DT_FOREACH_CHILD(DT_PATH(leds), LED_STATES)
};

/*
 * GPIO pin numbers (for up to 5 LED pins)
 */
static const gpio_pin_t led_pins[] = {
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 0, pin),
#if NUM_LED_PINS >= 2
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 1, pin),
#endif
#if NUM_LED_PINS >= 3
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 2, pin),
#endif
#if NUM_LED_PINS >= 4
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 3, pin),
#endif
#if NUM_LED_PINS >= 5
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 4, pin),
#endif
};

/*
 * GPIO port labels (for up to 5 LED pins)
 */
static const char *const led_ports[] = {
    DT_PROP_BY_PHANDLE_IDX(DT_PATH(leds), gpios, 0, label),
#if NUM_LED_PINS >= 2
    DT_PROP_BY_PHANDLE_IDX(DT_PATH(leds), gpios, 1, label),
#endif
#if NUM_LED_PINS >= 3
    DT_PROP_BY_PHANDLE_IDX(DT_PATH(leds), gpios, 2, label),
#endif
#if NUM_LED_PINS >= 4
    DT_PROP_BY_PHANDLE_IDX(DT_PATH(leds), gpios, 3, label),
#endif
#if NUM_LED_PINS >= 5
    DT_PROP_BY_PHANDLE_IDX(DT_PATH(leds), gpios, 4, label),
#endif
};

/*
 * GPIO pin flags (for up to 5 LED pins)
 */
static const gpio_flags_t led_flags[] = {
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 0, flags),
#if NUM_LED_PINS >= 2
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 1, flags),
#endif
#if NUM_LED_PINS >= 3
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 2, flags),
#endif
#if NUM_LED_PINS >= 4
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 3, flags),
#endif
#if NUM_LED_PINS >= 5
    DT_PHA_BY_IDX(DT_PATH(leds), gpios, 4, flags),
#endif
};

static int led_states[NUM_LEDS];                // must be one of enum LedState
static int trigger_timeout[NUM_LEDS];           // seconds until trigger should end

static bool blink_state = true;                 // global state of all blinking LEDs

static bool charging = false;

void leds_update_thread()
{
#ifndef UNIT_TEST
    unsigned int led_count = 0;
    int flicker_count = 0;
    bool flicker_state = true;
    const struct device *led_devs[NUM_LED_PINS];

    int wdt_channel = watchdog_register(1000);

    for (int pin = 0; pin < NUM_LED_PINS; pin++) {
        led_devs[pin] = device_get_binding(led_ports[pin]);
    }

    leds_init(true);

    while (true) {

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
            ))
        {
            for (unsigned int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
                switch (led_pin_states[led_count][pin_number]) {
                    case PIN_HIGH:
                        gpio_pin_configure(led_devs[pin_number], led_pins[pin_number],
                            led_flags[pin_number] | GPIO_OUTPUT);
                        gpio_pin_set(led_devs[pin_number], led_pins[pin_number], 1);
                        break;
                    case PIN_LOW:
                        gpio_pin_configure(led_devs[pin_number], led_pins[pin_number],
                            led_flags[pin_number] | GPIO_OUTPUT);
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
            for (unsigned int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
                gpio_pin_configure(led_devs[pin_number], led_pins[pin_number], GPIO_INPUT);
            }
        }

        led_count++;
        flicker_count++;

        k_sleep(K_MSEC(SLEEP_TIME_MS));
    }
#endif  // UNIT_TEST
}

void leds_init(bool enabled)
{
    for (unsigned int led = 0; led < NUM_LEDS; led++) {
        if (enabled) {
            led_states[led] = LED_STATE_ON;
            trigger_timeout[led] = 0;      // switched off the first time the main loop is called
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
}

void leds_set(unsigned int led, bool enabled, int timeout)
{
    if (led >= NUM_LEDS || led < 0) {
        return;
    }

    if (enabled) {
        led_states[led] = LED_STATE_ON;
    }
    else {
        led_states[led] = LED_STATE_OFF;
    }

    trigger_timeout[led] = timeout;
}

void leds_on(unsigned int led, int timeout)
{
    leds_set(led, true, timeout);
}

void leds_off(unsigned int led)
{
    leds_set(led, false, -1);
}

void leds_blink(unsigned int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_BLINK;
        trigger_timeout[led] = timeout;
    }
}

void leds_flicker(unsigned int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_FLICKER;
        trigger_timeout[led] = timeout;
    }
}

void leds_update_1s()
{
    blink_state = !blink_state;

    for (unsigned int led = 0; led < NUM_LEDS; led++) {
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
    for (unsigned int led = 0; led < NUM_LEDS; led++) {
        led_states[led] = (led % 2) ^ blink_state;
    }
    blink_state = !blink_state;
}

void leds_update_soc(int soc, bool load_off_low_soc)
{
    // blink SOC or power LED when charger (DC/DC or PWM switch) is enabled
    int blink_chg = (charging) ? LED_STATE_BLINK : LED_STATE_ON;

#if LED_EXISTS(pwr)
    led_states[LED_POS(pwr)] = blink_chg;
    trigger_timeout[LED_POS(pwr)] = -1;
#elif LED_EXISTS(soc_3)  // 3-bar SOC gauge
    trigger_timeout[LED_POS(soc_1)] = -1;
    trigger_timeout[LED_POS(soc_2)] = -1;
    trigger_timeout[LED_POS(soc_3)] = -1;
    if (soc > 80 && !load_off_low_soc) {
        led_states[LED_POS(soc_1)] = blink_chg;
        led_states[LED_POS(soc_2)] = LED_STATE_ON;
        led_states[LED_POS(soc_3)] = LED_STATE_ON;
    }
    else if (soc > 20 && !load_off_low_soc) {
        led_states[LED_POS(soc_1)] = LED_STATE_OFF;
        led_states[LED_POS(soc_2)] = blink_chg;
        led_states[LED_POS(soc_3)] = LED_STATE_ON;
    }
    else {
        led_states[LED_POS(soc_1)] = LED_STATE_OFF;
        led_states[LED_POS(soc_2)] = LED_STATE_OFF;
        led_states[LED_POS(soc_3)] = blink_chg;
    }
#endif
}

#ifndef UNIT_TEST
K_THREAD_DEFINE(leds_thread, 256, leds_update_thread, NULL, NULL, NULL,	4, 0, 100);
#endif
