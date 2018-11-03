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

#include "bat_charger.h"
#include "config.h"
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

// weak function to be overridden by user in config.cpp
WEAK void battery_init_user(battery_t *bat, battery_user_settings_t *bat_user) {;}

void battery_init(battery_t *bat, battery_user_settings_t *bat_user)
{
    // core battery pack specification (set in config.h)
    bat->type = BATTERY_TYPE;
    bat->num_cells = BATTERY_NUM_CELLS; 
    bat->capacity = BATTERY_CAPACITY;
    bat->num_batteries = 1;             // initialize with only one battery in series

    // common values for all batteries
    bat->charge_current_max = bat->capacity;        // 1C should be safe for all batteries

    bat->time_limit_recharge = 60;              // sec
    bat->time_limit_CV = 120*60;                // sec

    switch (bat->type)
    {
    case BAT_TYPE_FLOODED:
    case BAT_TYPE_AGM:
    case BAT_TYPE_GEL:
        bat->cell_voltage_max = 2.4;
        bat->cell_voltage_recharge = 2.3;

        bat->cell_voltage_load_disconnect = 1.95;
        bat->cell_voltage_load_reconnect = 2.1;
        bat->cell_voltage_absolute_min = 1.8;

        bat->cell_ocv_full = 2.2;
        bat->cell_ocv_empty = 1.9;

        bat->current_cutoff_CV = bat->capacity / 5.0;

        bat->trickle_enabled = true;
        bat->time_trickle_recharge = 30*60;
        // http://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
        bat->cell_voltage_trickle = (bat->type == BAT_TYPE_FLOODED) ? 2.25 : 2.3;

        bat->equalization_enabled = false;
        bat->cell_voltage_equalization = 2.5;
        bat->time_limit_equalization = 60*60;
        bat->current_limit_equalization = (1.0 /7.0) * bat->capacity;
        bat->equalization_trigger_time = 8;         // weeks
        bat->equalization_trigger_deep_cycles = 10; // times

        // not yet implemented...
        bat->temperature_compensation = -0.003;     // -3 mV/°C/cell
        break;

    case BAT_TYPE_LFP:
        bat->cell_voltage_max = 3.55;               // CV voltage
        bat->cell_voltage_recharge = 3.35;

        bat->cell_voltage_load_disconnect = 3.0;
        bat->cell_voltage_load_reconnect  = 3.15;
        bat->cell_voltage_absolute_min = 2.0;

        bat->cell_ocv_full = 3.4;       // will give really bad SOC calculation
        bat->cell_ocv_empty = 3.0;      // because of flat OCV of LFP cells...

        bat->current_cutoff_CV = bat->capacity / 10;    // C/10 cut-off at end of CV phase by default

        bat->trickle_enabled = false;
        bat->equalization_enabled = false;
        bat->temperature_compensation = 0.0;
        break;

    case BAT_TYPE_NMC:
    case BAT_TYPE_NMC_HV:
        bat->cell_voltage_max = (bat->type == BAT_TYPE_NMC_HV) ? 4.35 : 4.20;
        bat->cell_voltage_recharge = 3.9;

        bat->cell_voltage_load_disconnect = 3.3;
        bat->cell_voltage_load_reconnect  = 3.6;
        bat->cell_voltage_absolute_min = 2.5;

        bat->cell_ocv_full = 4.0;
        bat->cell_ocv_empty = 3.0;

        bat->current_cutoff_CV = bat->capacity / 10;    // C/10 cut-off at end of CV phase by default

        bat->trickle_enabled = false;
        bat->equalization_enabled = false;
        bat->temperature_compensation = 0.0;
        break;

    case BAT_TYPE_NONE:
    case BAT_TYPE_CUSTOM:
        break;
    }

    // initialize bat_user
    bat_user->capacity                      = bat->capacity;
    bat_user->voltage_max                   = bat->num_cells * bat->cell_voltage_max;
    bat_user->voltage_recharge              = bat->num_cells * bat->cell_voltage_recharge;
    bat_user->voltage_load_reconnect        = bat->num_cells * bat->cell_voltage_load_reconnect;
    bat_user->voltage_load_disconnect       = bat->num_cells * bat->cell_voltage_load_disconnect;
    bat_user->voltage_absolute_min          = bat->num_cells * bat->cell_voltage_absolute_min;
    bat_user->charge_current_max            = bat->charge_current_max;
    bat_user->current_cutoff_CV             = bat->current_cutoff_CV;
    bat_user->time_limit_CV                 = bat->time_limit_CV;
    bat_user->trickle_enabled               = bat->trickle_enabled;
    bat_user->time_trickle_recharge         = bat->time_trickle_recharge;
    bat_user->temperature_compensation      = bat->temperature_compensation;

    // call user function (defined in config.cpp to overwrite above initializations)
    battery_init_user(bat, bat_user);
}

// checks settings in bat_user for plausibility and writes them to bat if everything is fine
bool battery_user_settings(battery_t *bat, battery_user_settings_t *bat_user)
{
    // things to check:
    // - cell_voltage_XXX in the range of what the charger can handle?
    // - load_disconnect/reconnect hysteresis makes sense?
    // - cutoff current not extremely low/high
    // - capacity plausible

    if (bat_user->voltage_max > LOW_SIDE_VOLTAGE_MAX &&
        bat_user->voltage_load_reconnect > (bat_user->voltage_load_disconnect + 0.8) &&
        bat_user->voltage_recharge < (bat_user->voltage_max - 0.6) &&
        bat_user->voltage_recharge > (bat_user->voltage_load_disconnect + 1) &&
        bat_user->voltage_load_disconnect > (bat_user->voltage_absolute_min + 0.5) && 
        bat_user->current_cutoff_CV < (bat_user->capacity / 9.0) &&        // C/10 or lower allowed
        bat_user->current_cutoff_CV > 0.01 &&
        (bat_user->trickle_enabled == false || 
            (bat_user->voltage_trickle < bat_user->voltage_max && 
             bat_user->voltage_trickle > bat_user->voltage_load_disconnect))
       ) {

        // TODO: stop DC/DC

        bat->type                           = BAT_TYPE_CUSTOM;
        bat->num_cells                      = 1;    // custom battery defines voltages on battery level
        bat->capacity                       = bat_user->capacity;
        bat->cell_voltage_max               = bat_user->voltage_max;
        bat->cell_voltage_recharge          = bat_user->voltage_recharge;
        bat->cell_voltage_load_reconnect    = bat_user->voltage_load_reconnect;
        bat->cell_voltage_load_disconnect   = bat_user->voltage_load_disconnect;
        bat->cell_voltage_absolute_min      = bat_user->voltage_absolute_min;
        bat->charge_current_max             = bat_user->charge_current_max;
        bat->current_cutoff_CV              = bat_user->current_cutoff_CV;
        bat->time_limit_CV                  = bat_user->time_limit_CV;
        bat->trickle_enabled                = bat_user->trickle_enabled;
        bat->time_trickle_recharge          = bat_user->time_trickle_recharge;
        bat->temperature_compensation       = bat_user->temperature_compensation;

        // TODO:
        // - update also DC/DC etc. (now this function only works at system startup)
        // - restart DC/DC

        return true;
    }

    return false;
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

//----------------------------------------------------------------------------
// timespan in seconds since last call
void battery_update_energy(battery_t *bat, float voltage, float current, int timespan)
{
    // static variables so that the are not reset during each function call
    static int seconds_zero_solar = 0;
    static int seconds_day = 0;
    static int day_counter = 0;
    static bool nighttime = false;

    float dcdc_power = voltage * current;

    if (dcdc_power < 0.1) {
        seconds_zero_solar++;
    }
    else {
        seconds_zero_solar = 0;
    }

    // detect night time for reset of daily counter in the morning
    if (seconds_zero_solar > 60*60*5) {
        nighttime = true;
    }

    if (nighttime && dcdc_power > 0.1) {
        nighttime = false;
        bat->input_Wh_total += bat->input_Wh_day;
        bat->output_Wh_total += bat->output_Wh_day;
        bat->input_Wh_day = 0.0;
        bat->output_Wh_day = 0.0;
        seconds_day = 0;
        day_counter++;
        //eeprom_store_data(&ts);
    }
    else {
        seconds_day++;
    }

    bat->input_Wh_day += dcdc_power * timespan / 3600.0;
    bat->output_Wh_day += load.current * ls_port.voltage * timespan / 3600.0;
 
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
