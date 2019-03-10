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

void leds_init();
void led_timer_start(int freq_Hz);
void charlieplexing();

void update_dcdc_led(bool enabled);
void update_rxtx_led();
void trigger_tx_led();
void trigger_rx_led();
void update_soc_led(int soc);
void toggle_led_blink();

#endif /* LEDS_H */