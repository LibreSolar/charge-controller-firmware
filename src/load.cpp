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

#include "load.h"
#include "pcb.h"
#include "hardware.h"

void load_init(load_output_t *load)
{
    load->current_max = LOAD_CURRENT_MAX;
    load->enabled = false;
    load->enabled_target = true;
    load->usb_enabled_target = true;
    hw_load_switch(false);
    hw_usb_out(false);
    load->state = LOAD_STATE_DISABLED;
    load->usb_state = USB_STATE_DISABLED;
}


void usb_state_machine(load_output_t *load)
{
    switch (load->usb_state) {
    case USB_STATE_DISABLED:
        if (load->enabled && load->usb_enabled_target == true) {
            hw_usb_out(true);
            load->usb_state = USB_STATE_ON;
        }
        break;
    case USB_STATE_ON:
        // currently still same cut-off SOC limit as the load
        if (load->state == LOAD_STATE_OFF_LOW_SOC) {
            hw_usb_out(false);
            load->usb_state = USB_STATE_OFF_LOW_SOC;
        }
        else if (load->state == LOAD_STATE_OFF_OVERCURRENT) {
            hw_usb_out(false);
            load->usb_state = USB_STATE_OFF_OVERCURRENT;
        }
        else if (load->usb_enabled_target == false) {
            hw_usb_out(false);
            load->usb_state = USB_STATE_DISABLED;
        }
        break;
    case USB_STATE_OFF_LOW_SOC:
        // currently still same cut-off SOC limit as the load
        if (load->state == LOAD_STATE_ON) {
            if (load->usb_enabled_target == true) {
                hw_usb_out(true);
                load->usb_state = USB_STATE_ON;
            }
            else {
                load->usb_state = USB_STATE_DISABLED;
            }
        }
        break;
    case USB_STATE_OFF_OVERCURRENT:
        if (load->state != LOAD_STATE_OFF_OVERCURRENT) {
            load->usb_state = USB_STATE_DISABLED;
        }
        break;
    }
}

void load_state_machine(load_output_t *load, bool source_enabled)
{
    //printf("Load State: %d\n", load->state);
    switch (load->state) {
    case LOAD_STATE_DISABLED:
        if (source_enabled == true && load->enabled_target == true) {
            hw_load_switch(true);
            load->enabled = true;
            load->state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_ON:
        if (load->enabled_target == false) {
            hw_load_switch(false);
            load->enabled = false;
            load->state = LOAD_STATE_DISABLED;
        }
        else if (source_enabled == false) {
            hw_load_switch(false);
            load->enabled = false;
            load->state = LOAD_STATE_OFF_LOW_SOC;
        }
        break;
    case LOAD_STATE_OFF_LOW_SOC:
        if (source_enabled == true) {
            if (load->enabled_target == true) {
                hw_load_switch(true);
                load->enabled = true;
                load->state = LOAD_STATE_ON;
            }
            else {
                load->state = LOAD_STATE_DISABLED;
            }
        }
        break;
    case LOAD_STATE_OFF_OVERCURRENT:
        if (time(NULL) > load->overcurrent_timestamp + 30*60) {         // wait 5 min (TODO: make configurable)
            load->state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    }

    usb_state_machine(load);
}

// this function is called more often than the state machine
void load_check_overcurrent(load_output_t *load)
{
    if (load->current > load->current_max) {
        hw_load_switch(false);
        hw_usb_out(false);
        load->enabled = false;
        load->state = LOAD_STATE_OFF_OVERCURRENT;
        load->overcurrent_timestamp = time(NULL);
    }
}

