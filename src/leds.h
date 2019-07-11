/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
enum led_state_t {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK,
    LED_STATE_FLICKER
};

/** Initialize LEDs and start timer for charlieplexing
 */
void leds_init(bool enabled = true);

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