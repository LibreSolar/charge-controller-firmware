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

#include "battery.h"
#include "config.h"
#include "pcb.h"

#include "power_port.h"
#include "load.h"
#include "log.h"

#include <math.h>       // for fabs function

// ToDo: Remove global definitions!
extern power_port_t ls_port;
extern power_port_t hs_port;
extern load_output_t load;
extern log_data_t log_data;

void battery_conf_init(battery_conf_t *bat, bat_type type, int num_cells, float nominal_capacity)
{
    bat->nominal_capacity = nominal_capacity;

    // common values for all batteries
    bat->charge_current_max = bat->nominal_capacity;   // 1C should be safe for all batteries

    bat->time_limit_recharge = 60;              // sec
    bat->time_limit_CV = 120*60;                // sec

    bat->charge_temp_max = 50;
    bat->charge_temp_min = -10;
    bat->discharge_temp_max = 50;
    bat->discharge_temp_min = -10;

    switch (type)
    {
    case BAT_TYPE_FLOODED:
    case BAT_TYPE_AGM:
    case BAT_TYPE_GEL:
        bat->voltage_max = num_cells * 2.4;
        bat->voltage_recharge = num_cells * 2.3;

        // Cell-level thresholds based on EN 62509:2011 (both thresholds current-compensated)
        bat->voltage_load_disconnect = num_cells * 1.95;
        bat->voltage_load_reconnect = num_cells * 2.05;     // maybe increase to 2.10, if hysteresis observed?
        bat->internal_resistance = num_cells * (1.95 - 1.80) / LOAD_CURRENT_MAX;    // assumption: Battery selection matching charge controller

        bat->voltage_absolute_min = num_cells * 1.8;

        bat->ocv_full = num_cells * 2.15;
        bat->ocv_empty = num_cells * 1.95;

        // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
        bat->current_cutoff_CV = bat->nominal_capacity * 0.04;  // 3-5 % of C/1

        bat->trickle_enabled = true;
        bat->time_trickle_recharge = 30*60;
        // Values as suggested in EN 62509:2011
        bat->voltage_trickle = num_cells * ((type == BAT_TYPE_FLOODED) ? 2.35 : 2.3);

        // Enable for flooded batteries only, according to
        // https://discoverbattery.com/battery-101/equalizing-flooded-batteries-only
        bat->equalization_enabled = false;
        // Values as suggested in EN 62509:2011
        bat->voltage_equalization = num_cells * ((type == BAT_TYPE_FLOODED) ? 2.50 : 2.45);
        bat->time_limit_equalization = 60*60;
        bat->current_limit_equalization = (1.0 / 7.0) * bat->nominal_capacity;
        bat->equalization_trigger_time = 8;         // weeks
        bat->equalization_trigger_deep_cycles = 10; // times

        // not yet implemented...
        bat->temperature_compensation = -0.003;     // -3 mV/°C/cell
        break;

    case BAT_TYPE_LFP:
        bat->voltage_max = num_cells * 3.55;               // CV voltage
        bat->voltage_recharge = num_cells * 3.35;

        bat->voltage_load_disconnect = num_cells * 3.0;
        bat->voltage_load_reconnect  = num_cells * 3.15;
        bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;  // 5% voltage drop at max current
        bat->voltage_absolute_min = num_cells * 2.0;

        bat->ocv_full = num_cells * 3.4;       // will give really bad SOC calculation
        bat->ocv_empty = num_cells * 3.0;      // because of flat OCV of LFP cells...

        bat->current_cutoff_CV = bat->nominal_capacity / 10;    // C/10 cut-off at end of CV phase by default

        bat->trickle_enabled = false;
        bat->equalization_enabled = false;
        bat->temperature_compensation = 0.0;
        bat->charge_temp_min = 0;
        break;

    case BAT_TYPE_NMC:
    case BAT_TYPE_NMC_HV:
        bat->voltage_max = (type == BAT_TYPE_NMC_HV) ? 4.35 : 4.20;
        bat->voltage_recharge = num_cells * 3.9;

        bat->voltage_load_disconnect = num_cells * 3.3;
        bat->voltage_load_reconnect  = num_cells * 3.6;
        bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;  // 5% voltage drop at max current

        bat->voltage_absolute_min = num_cells * 2.5;

        bat->ocv_full = num_cells * 4.0;
        bat->ocv_empty = num_cells * 3.0;

        bat->current_cutoff_CV = bat->nominal_capacity / 10;    // C/10 cut-off at end of CV phase by default

        bat->trickle_enabled = false;
        bat->equalization_enabled = false;
        bat->temperature_compensation = 0.0;
        bat->charge_temp_min = 0;
        break;

    case BAT_TYPE_NONE:
        break;
    }
}

// checks settings in bat_conf for plausibility
bool battery_conf_check(battery_conf_t *bat_conf)
{
    // things to check:
    // - load_disconnect/reconnect hysteresis makes sense?
    // - cutoff current not extremely low/high
    // - capacity plausible

    /*
    printf("battery_conf_check: %d %d %d %d %d %d %d\n",
        bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.6),
        bat_conf->voltage_recharge < (bat_conf->voltage_max - 0.4),
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1),
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4),
        bat_conf->current_cutoff_CV < (bat_conf->nominal_capacity / 10.0),    // C/10 or lower allowed
        bat_conf->current_cutoff_CV > 0.01,
        (bat_conf->trickle_enabled == false ||
            (bat_conf->voltage_trickle < bat_conf->voltage_max &&
             bat_conf->voltage_trickle > bat_conf->voltage_load_disconnect))
    );
    */

    return
       (bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.6) &&
        bat_conf->voltage_recharge < (bat_conf->voltage_max - 0.4) &&
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1) &&
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4) &&
        bat_conf->internal_resistance < bat_conf->voltage_load_disconnect * 0.1 / LOAD_CURRENT_MAX &&       // max. 10% drop
        bat_conf->wire_resistance < bat_conf->voltage_max * 0.03 / LOAD_CURRENT_MAX &&                      // max. 3% loss
        bat_conf->current_cutoff_CV < (bat_conf->nominal_capacity / 10.0) &&    // C/10 or lower allowed
        bat_conf->current_cutoff_CV > 0.01 &&
        (bat_conf->trickle_enabled == false ||
            (bat_conf->voltage_trickle < bat_conf->voltage_max &&
             bat_conf->voltage_trickle > bat_conf->voltage_load_disconnect))
       );
}

void battery_conf_overwrite(battery_conf_t *source, battery_conf_t *destination, battery_state_t *bat_state)
{
    // TODO: stop DC/DC

    destination->voltage_max                    = source->voltage_max;
    destination->voltage_recharge               = source->voltage_recharge;
    destination->voltage_load_reconnect         = source->voltage_load_reconnect;
    destination->voltage_load_disconnect        = source->voltage_load_disconnect;
    destination->voltage_absolute_min           = source->voltage_absolute_min;
    destination->charge_current_max             = source->charge_current_max;
    destination->current_cutoff_CV              = source->current_cutoff_CV;
    destination->time_limit_CV                  = source->time_limit_CV;
    destination->trickle_enabled                = source->trickle_enabled;
    destination->voltage_trickle                = source->voltage_trickle;
    destination->time_trickle_recharge          = source->time_trickle_recharge;
    destination->charge_temp_max                = source->charge_temp_max;
    destination->charge_temp_min                = source->charge_temp_min;
    destination->discharge_temp_max             = source->discharge_temp_max;
    destination->discharge_temp_min             = source->discharge_temp_min;
    destination->temperature_compensation       = source->temperature_compensation;
    destination->internal_resistance            = source->internal_resistance;
    destination->wire_resistance                = source->wire_resistance;

    // reset Ah counter and SOH if battery nominal capacity was changed
    if (destination->nominal_capacity != source->nominal_capacity) {
        destination->nominal_capacity = source->nominal_capacity;
        if (bat_state != NULL) {
            bat_state->discharged_Ah = 0;
            bat_state->usable_capacity = 0;
            bat_state->soh = 0;
        }
    }

    // TODO:
    // - update also DC/DC etc. (now this function only works at system startup)
    // - restart DC/DC
}


void battery_state_init(battery_state_t *bat_state)
{
    bat_state->num_batteries = 1;             // initialize with only one battery in series
    bat_state->soh = 100;                     // assume new battery
    bat_state->temperature = 25.0;
}

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void battery_update_energy(battery_state_t *bat, float bat_voltage, float bat_current, float dcdc_current, float load_current)
{
    // static variables so that it is not reset for each function call
    static int seconds_zero_solar = 0;

    // stores the input/output energy status of previous day and to add
    // xxx_day_Wh only once per day and increase accuracy
    static uint32_t solar_in_total_Wh_prev = log_data.solar_in_total_Wh;
    static uint32_t load_out_total_Wh_prev = log_data.load_out_total_Wh;
    static uint32_t bat_chg_total_Wh_prev = bat->chg_total_Wh;
    static uint32_t bat_dis_total_Wh_prev = bat->dis_total_Wh;

    if (hs_port.voltage < ls_port.voltage) {
        seconds_zero_solar += 1;
    }
    else {
        // solar voltage > battery voltage after 5 hours of night time means sunrise in the morning
        // --> reset daily energy counters
        if (seconds_zero_solar > 60*60*5) {
            //printf("Night!\n");
            log_data.day_counter++;
            solar_in_total_Wh_prev = log_data.solar_in_total_Wh;
            load_out_total_Wh_prev = log_data.load_out_total_Wh;
            bat_chg_total_Wh_prev = bat->chg_total_Wh;
            bat_dis_total_Wh_prev = bat->dis_total_Wh;
            log_data.solar_in_day_Wh = 0.0;
            log_data.load_out_day_Wh = 0.0;
            bat->chg_total_Wh = 0.0;
            bat->dis_total_Wh = 0.0;
        }
        seconds_zero_solar = 0;
    }

    float bat_energy = bat_voltage * bat_current / 3600.0;  // timespan = 1s, so no multiplication with time
    if (bat_energy > 0) {
        bat->chg_day_Wh += bat_energy;
    }
    else {
        bat->dis_day_Wh += -(bat_energy);
    }
    bat->chg_total_Wh = bat_chg_total_Wh_prev + (bat->chg_day_Wh > 0 ? bat->chg_day_Wh : 0);
    bat->dis_total_Wh = bat_dis_total_Wh_prev + (bat->dis_day_Wh > 0 ? bat->dis_day_Wh : 0);
    bat->discharged_Ah += (load_current - bat_current) / 3600.0;

    log_data.solar_in_day_Wh += bat_voltage * dcdc_current / 3600.0;    // timespan = 1s, so no multiplication with time
    log_data.load_out_day_Wh += load.current * ls_port.voltage / 3600.0;
    log_data.solar_in_total_Wh = solar_in_total_Wh_prev + (log_data.solar_in_day_Wh > 0 ? log_data.solar_in_day_Wh : 0);
    log_data.load_out_total_Wh = load_out_total_Wh_prev + (log_data.load_out_day_Wh > 0 ? log_data.load_out_day_Wh : 0);
}

void battery_update_soc(battery_conf_t *bat_conf, battery_state_t *bat_state, float voltage, float current)
{
    static int soc_filtered = 0;       // SOC / 100 for better filtering

    if (fabs(current) < 0.2) {
        int soc_new = (int)((voltage - bat_conf->ocv_empty) /
                   (bat_conf->ocv_full - bat_conf->ocv_empty) * 10000.0);

        if (soc_new > 10000) {
            soc_new = 10000;
        }
        else if (soc_new < 0) {
            soc_new = 0;
        }

        if (soc_new > 500 && soc_filtered == 0) {
            // bypass filter during initialization
            soc_filtered = soc_new;
        }
        else {
            // filtering to adjust SOC very slowly
            soc_filtered += (soc_new - soc_filtered) / 100;
        }
        bat_state->soc = soc_filtered / 100;
    }
}