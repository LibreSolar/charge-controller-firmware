/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LEDS_H
#define LEDS_H

/**
 * @file
 *
 * @brief
 * Control of status LEDs with charlieplexing
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

/*
 * Find out if LED was specified in board devicetree file
 */
#define LED_EXISTS(node) DT_NODE_EXISTS(DT_CHILD(DT_PATH(leds), node))

#define LED_TIMEOUT_INFINITE (-1)

/*
 * Find out the position of an LED in the LED states array (using below enum)
 */
#define LED_POS(node) DT_N_S_leds_S_##node##_LED_POS

/*
 * Creates a unique name used by LED_POS macro for below enum
 */
#define LED_ENUM(node) node##_LED_POS,

/*
 * Enum for numbering of LEDs in the order specified in board.dts
 */
// cppcheck-suppress syntaxError
enum {
    DT_FOREACH_CHILD(DT_PATH(leds), LED_ENUM)
    NUM_LEDS          // trick to get the number of elements
};

/**
 * LED states
 */
enum LedState {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK,
    LED_STATE_FLICKER
};

/**
 * Pin states
 */
enum PinState {
    PIN_LOW = 0,
    PIN_HIGH,
    PIN_FLOAT
};

/**
 * Initialize LEDs (called at the beginning of leds_update_thread)
 */
void leds_init(bool enabled);

/**
 * Main thread for LED control. Performs pin initializations and charlieplexing at 60 Hz
 */
void leds_update_thread();

/**
 * Enables/disables dedicated charging LED if existing or blinks SOC LED when solar power is coming
 * in.
 */
void leds_set_charging(bool enabled);

/**
 * Enables/disables LED
 *
 * @param led Number of LED in array defined in PCB configuration
 * @param enabled LED is switched on if enabled is set to true
 * @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_set(unsigned int led, bool enabled, int timeout);

/**
 * Enable LED
 *
 * @param led Number of LED in array defined in PCB configuration
 * @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_on(unsigned int led, int timeout);

/**
 * Disable LED
 *
 * @param led Number of LED in array defined in PCB configuration
 */
void leds_off(unsigned int led);

/**
 * Blink LED
 *
 * @param led Number of LED in array defined in PCB configuration
 * @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_blink(unsigned int led, int timeout);

/**
 * Flicker LED
 *
 * @param led Number of LED in array defined in PCB configuration
 * @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_flicker(unsigned int led, int timeout);

/** Updates LED blink and timeout states, must be called every second
 */
void leds_update_1s();

/**
 * Update SOC LED bar (if existing)
 *
 * @param soc SOC in percent
 * @param load_off_low_soc Prevents showing two SOC LEDs if load is switched off because of low SOC
 */
void leds_update_soc(int soc, bool load_off_low_soc);

/**
 * Toggle between even and uneven LEDs switched on/off to create
 * annoying flashing in case of an error.
 */
void leds_toggle_error();

#ifdef __cplusplus
}
#endif

#endif /* LEDS_H */
