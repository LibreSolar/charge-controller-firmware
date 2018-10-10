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

#ifndef UNIT_TEST

#include "charger.h"

#include "half_bridge.h"

//#include "thingset.h"
#include "data_objects.h"
#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function

// private function
void _enter_state(battery_t* bat, int next_state)
{
    //printf("Enter State: %d\n", next_state);
    bat->time_state_changed = time(NULL);
    bat->state = next_state;
}

void battery_init(battery_t* bat, int type, int capacity_Ah)
{
    bat->num_cells = 6;     // for normal 12V lead acid batteries
    bat->capacity = capacity_Ah;

    // State: Standby
    bat->time_limit_recharge = 60;              // sec
    bat->cell_voltage_recharge = 2.3;           // V
    bat->cell_voltage_absolute_min = 1.8;       // V   (under this voltage, battery is considered damaged)

    bat->charge_current_max = 20;               // A        PCB maximum: 20 A
    bat->cell_voltage_max = 2.4;                // CV/absorption stage
    bat->time_limit_CV = 120*60;                // sec
    bat->current_cutoff_CV = 2.0;               // A

    bat->trickle_enabled = true;                // float/trickle state (not needed for li-ion chemistries)
    bat->cell_voltage_trickle = 2.3;            // target voltage for trickle charging of lead-acid batteries
    bat->time_trickle_recharge = 30*60;         // sec

    bat->equalization_enabled = false;          // only for lead acid
    bat->cell_voltage_equalization = 2.5;       // V
    bat->time_limit_equalization = 60*60;       // sec
    bat->current_limit_equalization = 1.0;      // A
    bat->equalization_trigger_time = 8;         // weeks
    bat->equalization_trigger_deep_cycles = 10; // times

    bat->cell_voltage_load_disconnect = 1.9;    // 1.95
    bat->cell_voltage_load_reconnect = 2.0;     // 2.2

    bat->cell_ocv_full = 2.2;
    bat->cell_ocv_empty = 1.9;

    // not yet implemented...
    bat->temperature_compensation = -0.003;     // -3 mV/°C/cell

    if (type == BAT_TYPE_FLOODED) {
        // http://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
        bat->cell_voltage_trickle = 2.25;
    }
    else if (type == BAT_TYPE_LFP) {    // 12V LiFePO4 battery
        bat->num_cells = 4;
        bat->cell_voltage_max = 3.55;               // CV voltage
        bat->current_cutoff_CV = capacity_Ah / 10;  // C/10 cut-off at end of CV phase
        bat->cell_voltage_recharge = 3.35;
        bat->trickle_enabled = false;
        bat->cell_voltage_load_disconnect = 3.0;
        bat->cell_voltage_load_reconnect  = 3.15;
        bat->temperature_compensation = 0.0;
    }
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
void charger_state_machine(dcdc_port_t *port, battery_t *bat, float voltage, float current)
{
    //printf("time_state_change = %d, time = %d, v_bat = %f, i_bat = %f\n", bat->time_state_changed, time(NULL), voltage, current);

    // Load management
    // battery port input state (i.e. battery discharging direction) defines load state
    if (port->input_allowed == true &&
        voltage < bat->cell_voltage_load_disconnect * bat->num_cells)
    {
        port->input_allowed = false;
        bat->num_deep_discharges++;
    }
    if (voltage >= bat->cell_voltage_load_reconnect * bat->num_cells) {
        port->input_allowed = true;
    }

    // state machine
    switch (bat->state) {
    case CHG_IDLE: {
        if  (voltage < bat->num_cells * bat->cell_voltage_recharge
                && (time(NULL) - bat->time_state_changed) > bat->time_limit_recharge)
        {
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            port->current_output_max = bat->charge_current_max;
            port->output_allowed = true;
            bat->full = false;
            _enter_state(bat, CHG_CC);
        }
        break;
    }
    case CHG_CC: {
        if (voltage > port->voltage_output_target) {
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            _enter_state(bat, CHG_CV);
        }
        break;
    }
    case CHG_CV: {
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

            if (bat->equalization_enabled) {
                // TODO: additional conditions!
                port->voltage_output_target = bat->num_cells * bat->cell_voltage_equalization;
                port->current_output_max = bat->current_limit_equalization;
                _enter_state(bat, CHG_EQUALIZATION);
            }
            else if (bat->trickle_enabled) {
                port->voltage_output_target = bat->num_cells * bat->cell_voltage_trickle;
                _enter_state(bat, CHG_TRICKLE);
            }
            else {
                port->current_output_max = 0;
                port->output_allowed = false;
                _enter_state(bat, CHG_IDLE);
            }
        }
        break;
    }
    case CHG_TRICKLE: {
        if (voltage >= port->voltage_output_target) {
            bat->time_voltage_limit_reached = time(NULL);
        }

        if (time(NULL) - bat->time_voltage_limit_reached > bat->time_trickle_recharge)
        {
            port->current_output_max = bat->charge_current_max;
            port->voltage_output_target = bat->num_cells * bat->cell_voltage_max;
            bat->full = false;
            _enter_state(bat, CHG_CC);
        }
        // assumtion: trickle does not harm the battery --> never go back to idle
        // (for Li-ion battery: disable trickle!)
        break;
    }
    }
}

// battery port input state (i.e. battery discharging direction) defines load state
void load_update(dcdc_port_t *bat_port, battery_t *spec)
{
    // TODO
}

void bat_calculate_soc(battery_t *bat, float voltage, float current)
{
    if (fabs(current) < 0.5) {
        bat->soc = (int)((voltage / bat->num_cells - bat->cell_ocv_empty) /
                   (bat->cell_ocv_full - bat->cell_ocv_empty) * 100.0);
        if (bat->soc > 100) {
            bat->soc = 100;
        }
        else if (bat->soc < 0) {
            bat->soc = 0;
        }
    }
}

#endif /* UNIT_TEST */
