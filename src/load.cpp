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

#include "mbed.h"
#include "data_objects.h"

#include "structs.h"
#include "pcb.h"

#include "load.h"
#include "hardware.h"

void load_init(load_output_t *load)
{
    load->current_max = LOAD_CURRENT_MAX;
    load->enabled = false;
    load->enabled_target = true;
    load->usb_enabled_target = true;
    hw_load_switch(false);
    hw_usb_out(false);
    load->state = LOAD_STATE_OFF;
}

void load_state_machine(load_output_t *load, bool source_enabled)
{
    switch (load->state) {
    case LOAD_STATE_ON:
        if (source_enabled == false || load->enabled_target == false) {
            hw_load_switch(false);
            hw_usb_out(false);
            load->enabled = false;
            load->state = LOAD_STATE_OFF;
        } else {
            hw_usb_out(load->usb_enabled_target);
        }
        break;
    case LOAD_STATE_OFF:
        if (source_enabled == true && load->enabled_target == true) {
            hw_load_switch(true);
            load->enabled = true;
            load->state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_OVERCURRENT:
        if (time(NULL) > load->overcurrent_timestamp + 30*60) {         // wait 5 min (TODO: make configurable)
            load->state = LOAD_STATE_OFF;   // switch to normal mode again
        }
        break;
    }
}

// this function is called more often than the state machine
void load_check_overcurrent(load_output_t *load)
{
    if (load->current > load->current_max) {
        hw_load_switch(false);
        hw_usb_out(false);
        load->enabled = false;
        load->state = LOAD_STATE_OVERCURRENT;
        load->overcurrent_timestamp = time(NULL);
    }
}

