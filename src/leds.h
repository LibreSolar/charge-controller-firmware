/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LEDS_H
#define LEDS_H

/** @file
 *
 * @brief
 * Control of status LEDs with charlieplexing
 */

/** LED state type
 */
enum LedState {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK,
    LED_STATE_FLICKER
};

/**
 * Initialize LEDs and start timer for charlieplexing. This function only needs to be called if
 * used in a single-thread application (mbed)
 */
void leds_init(bool enabled = true);

#ifdef __ZEPHYR__
/** Main thread for LED control in Zephyr, performs pin initializations and charlieplexing at 60 Hz
 */
void leds_update_thread();
#endif

/** Enables/disables dedicated charging LED if existing or
 *  blinks SOC LED when solar power is coming in.
 */
void leds_set_charging(bool enabled);

/** Enables/disables LED
 *
 *  @param led Number of LED in array defined in PCB configuration
 *  @param enabled LED is switched on if enabled is set to true
 *  @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_set(int led, bool enabled, int timeout = -1);

/** Enables LED
 *
 *  @param led Number of LED in array defined in PCB configuration
 *  @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_on(int led, int timeout = -1);

/** Disables LED
 *
 *  @param led Number of LED in array defined in PCB configuration
 */
void leds_off(int led);

/** Blinks LED
 *
 *  @param led Number of LED in array defined in PCB configuration
 *  @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_blink(int led, int timeout = -1);

/** Flickers LED
 *
 *  @param led Number of LED in array defined in PCB configuration
 *  @param timeout Defines for how long this state should be set (-1 for permanent setting)
 */
void leds_flicker(int led, int timeout = -1);

/** Updates LED blink and timeout states, must be called every second
 */
void leds_update_1s();

/** Updates SOC LED bar (if existing)
 *
 *  @param soc SOC in percent
 *  @param load_off_low_soc Prevents showing two SOC LEDs if load is switched off because of low SOC
 */
void leds_update_soc(int soc, bool load_off_low_soc = false);

/** Toggles between even and uneven LEDs switched on/off to create
 *  annoying flashing in case of an error.
 */
void leds_toggle_error();

#endif /* LEDS_H */