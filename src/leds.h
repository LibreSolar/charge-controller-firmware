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

/** Initialize LEDs and start timer for charlieplexing
 */
void leds_init(bool enabled = true);

/** Enables/disables dedicated charging LED if existing or
 *  blinks SOC LED when solar power is coming in.
 */
void leds_set_charging(bool enabled);

/** Enables/disables dedicated load output LED (if existing)
 */
void leds_set_load(bool enabled);

/** Updates SOC LED bar (if existing)
 */
void leds_update_soc(int soc);

/** Sets RX/TX LED to TX state (= constant on) for 2 seconds
 */
void leds_trigger_tx();

/** Sets RX/TX LED to RX state (= flicker) for 2 seconds
 */
void leds_trigger_rx();

/** Must be called regularly and sets flicker or on state of RX/TX LED
 */
void leds_update_rxtx();

/** Toggles blinking LEDs and must be called every second
 */
void leds_toggle_blink();

/** Toggles between even and uneven LEDs switched on/off to create
 *  annoying flashing in case of an error.
 */
void leds_toggle_error();

#endif /* LEDS_H */