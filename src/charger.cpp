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

#include "charger.h"
#include "config.h"
#include "pcb.h"
#include "half_bridge.h"
#include "log.h"

// private function
void _enter_state(battery_t* bat, int next_state)
{
    //printf("Enter State: %d\n", next_state);
    bat->time_state_changed = time(NULL);
    bat->state = next_state;
}

/*****************************************************************************
 *  Charger state machine

 ## Idle
 Initial state of the charge controller. If the solar voltage is high enough
 and the battery is not full, charging in CC mode is started.

 ## CC / bulk charging
 The battery is charged with maximum possible current (MPPT algorithm is
 active) until the CV voltage limit is reached.

 ## CV / absorption charging
 Lead-acid batteries are charged for some time using a slightly higher charge
 voltage. After a current cutoff limit or a time limit is reached, the charger
 goes into trickle or equalization mode for lead-acid batteries or back into
 Standby for Li-ion batteries.

 ## Trickle charging
 This mode is kept forever for a lead-acid battery and keeps the battery at
 full state of charge. If too much power is drawn from the battery, the
 charger switches back into CC / bulk charging mode.

 ## Equalization charging
 This mode is only used for lead-acid batteries after several deep-discharge
 cycles or a very long period of time with no equalization. Voltage is
 increased to 15V or above, so care must be taken for the other system
 components attached to the battery. (currently, no equalization charging is
 enabled in the software)

 */
void charger_state_machine(power_port_t *port, battery_t *bat, float voltage, float current)
{
    //printf("time_state_change = %d, time = %d, v_bat = %f, i_bat = %f\n", bat->time_state_changed, time(NULL), voltage, current);

    // Load management
    // battery port input state (i.e. battery discharging direction) defines load state
    if (port->input_allowed == true &&
        voltage < bat->cell_voltage_load_disconnect * bat->num_cells)
    {
        port->input_allowed = false;
        bat->num_deep_discharges++;

        if (bat->useable_capacity < 0.1) {
            bat->useable_capacity = bat->discharged_Ah;
        } else {
            // slowly adapt new measurements with low-pass filter
            bat->useable_capacity = 0.8 * bat->useable_capacity + 0.2 * bat->discharged_Ah;
        }

        // simple SOH estimation
        bat->soh = bat->useable_capacity / bat->nominal_capacity;
    }
    if (voltage >= bat->cell_voltage_load_reconnect * bat->num_cells) {
        port->input_allowed = true;
    }

    // state machine
    switch (bat->state) {
    case CHG_STATE_IDLE: {
        if  (voltage < bat->num_cells * bat->cell_voltage_recharge
                && (time(NULL) - bat->time_state_changed) > bat->time_limit_recharge)
        {
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            port->current_output_max = bat->charge_current_max;
            port->output_allowed = true;
            bat->full = false;
            _enter_state(bat, CHG_STATE_CC);
        }
        break;
    }
    case CHG_STATE_CC: {
        if (voltage > port->voltage_output_target) {
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            _enter_state(bat, CHG_STATE_CV);
        }
        break;
    }
    case CHG_STATE_CV: {
        if (voltage >= port->voltage_output_target) {
            bat->time_voltage_limit_reached = time(NULL);
        }

        // cut-off limit reached because battery full (i.e. CV mode still
        // reached by available solar power within last 2s) or CV period long enough?
        if ((current < bat->current_cutoff_CV && (time(NULL) - bat->time_voltage_limit_reached) < 2)
            || (time(NULL) - bat->time_state_changed) > bat->time_limit_CV)
        {
            bat->full = true;
            bat->num_full_charges++;
            bat->discharged_Ah = 0;         // reset coulomb counter

            if (bat->equalization_enabled) {
                // TODO: additional conditions!
                port->voltage_output_target = bat->num_cells * bat->cell_voltage_equalization;
                port->current_output_max = bat->current_limit_equalization;
                _enter_state(bat, CHG_STATE_EQUALIZATION);
            }
            else if (bat->trickle_enabled) {
                port->voltage_output_target = bat->num_cells * bat->cell_voltage_trickle;
                _enter_state(bat, CHG_STATE_TRICKLE);
            }
            else {
                port->current_output_max = 0;
                port->output_allowed = false;
                _enter_state(bat, CHG_STATE_IDLE);
            }
        }
        break;
    }
    case CHG_STATE_TRICKLE: {
        if (voltage >= port->voltage_output_target) {
            bat->time_voltage_limit_reached = time(NULL);
        }

        if (time(NULL) - bat->time_voltage_limit_reached > bat->time_trickle_recharge)
        {
            port->current_output_max = bat->charge_current_max;
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            bat->full = false;
            _enter_state(bat, CHG_STATE_CC);
        }
        // assumption: trickle does not harm the battery --> never go back to idle
        // (for Li-ion battery: disable trickle!)
        break;
    }
    }
}
