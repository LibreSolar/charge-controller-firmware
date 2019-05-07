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

#include "load.h"
#include "pcb.h"
#include "hardware.h"
#include "log.h"
#include <time.h>

void load_init(load_output_t *load)
{
    load->current_max = LOAD_CURRENT_MAX;
    //load->enabled = false;
    load->enabled_target = true;
    load->usb_enabled_target = true;
    hw_load_switch(false);
    hw_usb_out(false);
    load->switch_state = LOAD_STATE_DISABLED;
    load->usb_state = LOAD_STATE_DISABLED;
    load->junction_temperature = 25;
}


void usb_state_machine(load_output_t *load)
{
    switch (load->usb_state) {
    case LOAD_STATE_DISABLED:
        if (load->usb_enabled_target == true) {
            hw_usb_out(true);
            load->usb_state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_ON:
        // currently still same cut-off SOC limit as the load
        if (load->switch_state == LOAD_STATE_OFF_LOW_SOC) {
            hw_usb_out(false);
            load->usb_state = LOAD_STATE_OFF_LOW_SOC;
        }
        else if (load->switch_state == LOAD_STATE_OFF_OVERCURRENT) {
            hw_usb_out(false);
            load->usb_state = LOAD_STATE_OFF_OVERCURRENT;
        }
        else if (load->usb_enabled_target == false) {
            hw_usb_out(false);
            load->usb_state = LOAD_STATE_DISABLED;
        }
        break;
    case LOAD_STATE_OFF_LOW_SOC:
        // currently still same cut-off SOC limit as the load
        if (load->switch_state == LOAD_STATE_ON) {
            if (load->usb_enabled_target == true) {
                hw_usb_out(true);
                load->usb_state = LOAD_STATE_ON;
            }
            else {
                load->usb_state = LOAD_STATE_DISABLED;
            }
        }
        break;
    case LOAD_STATE_OFF_OVERCURRENT:
        if (load->switch_state != LOAD_STATE_OFF_OVERCURRENT) {
            load->usb_state = LOAD_STATE_DISABLED;
        }
        break;
    }
}

void load_state_machine(load_output_t *load, bool source_enabled)
{
    //printf("Load State: %d\n", load->switch_state);
    switch (load->switch_state) {
    case LOAD_STATE_DISABLED:
        if (source_enabled == true && load->enabled_target == true) {
            hw_load_switch(true);
            //load->enabled = true;
            load->switch_state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_ON:
        if (load->enabled_target == false) {
            hw_load_switch(false);
            //load->enabled = false;
            load->switch_state = LOAD_STATE_DISABLED;
        }
        else if (source_enabled == false) {
            hw_load_switch(false);
            //load->enabled = false;
            load->switch_state = LOAD_STATE_OFF_LOW_SOC;
        }
        break;
    case LOAD_STATE_OFF_LOW_SOC:
        if (source_enabled == true) {
            if (load->enabled_target == true) {
                hw_load_switch(true);
                //load->enabled = true;
                load->switch_state = LOAD_STATE_ON;
            }
            else {
                load->switch_state = LOAD_STATE_DISABLED;
            }
        }
        break;
    case LOAD_STATE_OFF_OVERCURRENT:
        if (time(NULL) > load->overcurrent_timestamp + 30*60) {         // wait 5 min (TODO: make configurable)
            load->switch_state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    case LOAD_STATE_OFF_OVERVOLTAGE:
        if (load->voltage < LOW_SIDE_VOLTAGE_MAX) {     // TODO: add hysteresis?
            load->switch_state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    }

    usb_state_machine(load);
}

extern log_data_t log_data;
extern float mcu_temp;

// this function is called more often than the state machine
void load_control(load_output_t *load)
{
    // junction temperature calculation model for overcurrent detection
    load->junction_temperature = load->junction_temperature + (
            mcu_temp - load->junction_temperature +
            load->current * load->current / (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) * (MOSFET_MAX_JUNCTION_TEMP - 25)
        ) / (MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY);

    if (load->junction_temperature > MOSFET_MAX_JUNCTION_TEMP) {
        hw_load_switch(false);
        hw_usb_out(false);
        //load->enabled = false;
        load->switch_state = LOAD_STATE_OFF_OVERCURRENT;
        load->overcurrent_timestamp = time(NULL);
    }

    static int debounce_counter = 0;
    if (load->voltage > LOW_SIDE_VOLTAGE_MAX) {
        debounce_counter++;
        if (debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
            hw_load_switch(false);
            hw_usb_out(false);
            //load->enabled = false;
            load->switch_state = LOAD_STATE_OFF_OVERVOLTAGE;
            load->overcurrent_timestamp = time(NULL);
            log_data.error_flags |= (1 << ERR_BAT_OVERVOLTAGE);
        }
    }
    debounce_counter = 0;
}
