/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifndef __LOAD_H_
#define __LOAD_H_

#include "mbed.h"

enum load_state {
    LOAD_STATE_ON, LOAD_STATE_OFF, LOAD_STATE_OVERCURRENT
};

/* Load Output type
 * 
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
typedef struct {
    load_state state;
    float current;              // actual measurement
    float current_max;          // maximum allowed current
    int overcurrent_timestamp;  // time at which an overcurrent event occured
    bool enabled;               // actual status
    bool enabled_target;        // target setting defined via communication 
                                // port (overruled if battery is empty)
    bool usb_enabled_target;    // same for USB output
} load_output_t;

void load_init(load_output_t *load);

void load_state_machine(load_output_t *load, bool source_enabled);

void load_check_overcurrent(load_output_t *load);

#endif /* __LOAD_H_ */
