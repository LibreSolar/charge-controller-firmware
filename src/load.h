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

#ifndef LOAD_H
#define LOAD_H

/** @file
 *
 * @brief
 * Load/USB output functions and data types
 */

#include <stdbool.h>
#include <stdint.h>

/** Load/USB output states
 *
 * Used for USB or normal load outputs connecting to battery via switch.
 */
enum load_state_t {
    LOAD_STATE_DISABLED = 0,        ///< Actively disabled
    LOAD_STATE_ON,                  ///< Normal state: On
    LOAD_STATE_OFF_LOW_SOC,         ///< Off to protect battery (overrules target setting)
    LOAD_STATE_OFF_OVERCURRENT      ///< Off to protect charge controller (overrules target setting)
};

/** Load output type
 *
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
typedef struct {
    uint16_t switch_state;      ///< Current state of load output switch
    uint16_t usb_state;         ///< Current state of USB output
    float current;              ///< actual measurement
    float current_max;          ///< maximum allowed current
    int overcurrent_timestamp;  ///< time at which an overcurrent event occured
    bool enabled;               ///< actual status
    bool enabled_target;        ///< target setting defined via communication port (overruled if battery is empty)
    bool usb_enabled_target;    ///< same for USB output
} load_output_t;

/** Initialize load_output_t struct
 */
void load_init(load_output_t *load);

/** State machine, called every second.
 */
void load_state_machine(load_output_t *load, bool source_enabled);

/** Overcurrent check function, should be called as often as possible
 */
void load_check_overcurrent(load_output_t *load);

#endif /* LOAD_H */
