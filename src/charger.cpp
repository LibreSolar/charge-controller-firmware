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
void _enter_state(battery_state_t* bat_state, int next_state)
{
    //printf("Enter State: %d\n", next_state);
    bat_state->time_state_changed = time(NULL);
    bat_state->chg_state = next_state;
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
void charger_state_machine(power_port_t *port, battery_conf_t *bat_conf, battery_state_t *bat_state, float voltage, float current)
{
    //printf("time_state_change = %d, time = %d, v_bat = %f, i_bat = %f\n", bat_state->time_state_changed, time(NULL), voltage, current);

    // Load management
    // battery port input state (i.e. battery discharging direction) defines load state
    if (port->input_allowed == true &&
        voltage < bat_conf->voltage_load_disconnect)
    {
        port->input_allowed = false;
        bat_state->num_deep_discharges++;

        if (bat_state->useable_capacity < 0.1) {
            bat_state->useable_capacity = bat_state->discharged_Ah;
        } else {
            // slowly adapt new measurements with low-pass filter
            bat_state->useable_capacity = 0.8 * bat_state->useable_capacity + 0.2 * bat_state->discharged_Ah;
        }

        // simple SOH estimation
        bat_state->soh = bat_state->useable_capacity / bat_conf->nominal_capacity;
    }
    if (voltage >= bat_conf->voltage_load_reconnect) {
        port->input_allowed = true;
    }

    // state machine
    switch (bat_state->chg_state) {
    case CHG_STATE_IDLE: {
        if  (voltage < bat_conf->voltage_recharge
                && (time(NULL) - bat_state->time_state_changed) > bat_conf->time_limit_recharge)
        {
            port->voltage_output_target = bat_conf->voltage_max;
            port->current_output_max = bat_conf->charge_current_max;
            port->output_allowed = true;
            bat_state->full = false;
            _enter_state(bat_state, CHG_STATE_CC);
        }
        break;
    }
    case CHG_STATE_CC: {
        if (voltage > port->voltage_output_target) {
            port->voltage_output_target = bat_conf->voltage_max;
            _enter_state(bat_state, CHG_STATE_CV);
        }
        break;
    }
    case CHG_STATE_CV: {
        if (voltage >= port->voltage_output_target) {
            bat_state->time_voltage_limit_reached = time(NULL);
        }

        // cut-off limit reached because battery full (i.e. CV mode still
        // reached by available solar power within last 2s) or CV period long enough?
        if ((current < bat_conf->current_cutoff_CV && (time(NULL) - bat_state->time_voltage_limit_reached) < 2)
            || (time(NULL) - bat_state->time_state_changed) > bat_conf->time_limit_CV)
        {
            bat_state->full = true;
            bat_state->num_full_charges++;
            bat_state->discharged_Ah = 0;         // reset coulomb counter

            if (bat_conf->equalization_enabled) {
                // TODO: additional conditions!
                port->voltage_output_target = bat_conf->voltage_equalization;
                port->current_output_max = bat_conf->current_limit_equalization;
                _enter_state(bat_state, CHG_STATE_EQUALIZATION);
            }
            else if (bat_conf->trickle_enabled) {
                port->voltage_output_target = bat_conf->voltage_trickle;
                _enter_state(bat_state, CHG_STATE_TRICKLE);
            }
            else {
                port->current_output_max = 0;
                port->output_allowed = false;
                _enter_state(bat_state, CHG_STATE_IDLE);
            }
        }
        break;
    }
    case CHG_STATE_TRICKLE: {
        if (voltage >= port->voltage_output_target) {
            bat_state->time_voltage_limit_reached = time(NULL);
        }

        if (time(NULL) - bat_state->time_voltage_limit_reached > bat_conf->time_trickle_recharge)
        {
            port->current_output_max = bat_conf->charge_current_max;
            port->voltage_output_target = bat_conf->voltage_max;
            bat_state->full = false;
            _enter_state(bat_state, CHG_STATE_CC);
        }
        // assumption: trickle does not harm the battery --> never go back to idle
        // (for Li-ion battery: disable trickle!)
        break;
    }
    }
}
