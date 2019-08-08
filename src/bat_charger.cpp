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

#include "bat_charger.h"
#include "config.h"
#include "pcb.h"

#include "dc_bus.h"
#include "load.h"
#include "log.h"

#include <math.h>       // for fabs function
#include <stdio.h>
#include <time.h>

void battery_conf_init(BatConf *bat, BatType type, int num_cells, float nominal_capacity)
{
    bat->nominal_capacity = nominal_capacity;

    // common values for all batteries
    bat->charge_current_max = bat->nominal_capacity;   // 1C should be safe for all batteries

    bat->time_limit_recharge = 60;              // sec
    bat->time_limit_topping = 120*60;                // sec

    bat->charge_temp_max = 50;
    bat->charge_temp_min = -10;
    bat->discharge_temp_max = 50;
    bat->discharge_temp_min = -10;

    switch (type)
    {
        case BAT_TYPE_FLOODED:
        case BAT_TYPE_AGM:
        case BAT_TYPE_GEL:
            bat->voltage_absolute_max = num_cells * 2.45;
            bat->voltage_topping = num_cells * 2.4;
            bat->voltage_recharge = num_cells * 2.3;

            // Cell-level thresholds based on EN 62509:2011 (both thresholds current-compensated)
            bat->voltage_load_disconnect = num_cells * 1.95;
            bat->voltage_load_reconnect = num_cells * 2.10;

            // assumption: Battery selection matching charge controller
            bat->internal_resistance = num_cells * (1.95 - 1.80) / LOAD_CURRENT_MAX;

            bat->voltage_absolute_min = num_cells * 1.6;

            // Voltages during idle (no charging/discharging current)
            bat->ocv_full = num_cells * ((type == BAT_TYPE_FLOODED) ? 2.10 : 2.15);
            bat->ocv_empty = num_cells * 1.90;

            // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
            bat->current_cutoff_topping = bat->nominal_capacity * 0.04;  // 3-5 % of C/1

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

            bat->temperature_compensation = -0.003;     // -3 mV/°C/cell
            break;

        case BAT_TYPE_LFP:
            bat->voltage_absolute_max = num_cells * 3.60;
            bat->voltage_topping = num_cells * 3.55;               // CV voltage
            bat->voltage_recharge = num_cells * 3.35;

            bat->voltage_load_disconnect = num_cells * 3.0;
            bat->voltage_load_reconnect  = num_cells * 3.15;

            // 5% voltage drop at max current
            bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;
            bat->voltage_absolute_min = num_cells * 2.0;

            bat->ocv_full = num_cells * 3.4;       // will give really nonlinear SOC calculation
            bat->ocv_empty = num_cells * 3.0;      // because of flat OCV of LFP cells...

            // C/10 cut-off at end of CV phase by default
            bat->current_cutoff_topping = bat->nominal_capacity / 10;

            bat->trickle_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0.0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_NMC:
        case BAT_TYPE_NMC_HV:
            bat->voltage_topping = num_cells * ((type == BAT_TYPE_NMC_HV) ? 4.35 : 4.20);
            bat->voltage_absolute_max = num_cells * (bat->voltage_topping + 0.05);
            bat->voltage_recharge = num_cells * 3.9;

            bat->voltage_load_disconnect = num_cells * 3.3;
            bat->voltage_load_reconnect  = num_cells * 3.6;

            // 5% voltage drop at max current
            bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;

            bat->voltage_absolute_min = num_cells * 2.5;

            bat->ocv_full = num_cells * 4.0;
            bat->ocv_empty = num_cells * 3.0;

            // C/10 cut-off at end of CV phase by default
            bat->current_cutoff_topping = bat->nominal_capacity / 10;

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
bool battery_conf_check(BatConf *bat_conf)
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
        bat_conf->current_cutoff_CV < (bat_conf->nominal_capacity / 10.0), // C/10 or lower allowed
        bat_conf->current_cutoff_CV > 0.01,
        (bat_conf->trickle_enabled == false ||
            (bat_conf->voltage_trickle < bat_conf->voltage_max &&
             bat_conf->voltage_trickle > bat_conf->voltage_load_disconnect))
    );
    */

    return
       (bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.4) &&
        bat_conf->voltage_recharge < (bat_conf->voltage_topping - 0.4) &&
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1) &&
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4) &&
        // max. 10% drop
        bat_conf->internal_resistance < bat_conf->voltage_load_disconnect * 0.1 / LOAD_CURRENT_MAX &&
        // max. 3% loss
        bat_conf->wire_resistance < bat_conf->voltage_topping * 0.03 / LOAD_CURRENT_MAX &&
        // C/10 or lower current cutoff allowed
        bat_conf->current_cutoff_topping < (bat_conf->nominal_capacity / 10.0) &&
        bat_conf->current_cutoff_topping > 0.01 &&
        (bat_conf->trickle_enabled == false ||
            (bat_conf->voltage_trickle < bat_conf->voltage_topping &&
             bat_conf->voltage_trickle > bat_conf->voltage_load_disconnect))
       );
}

void battery_conf_overwrite(BatConf *source, BatConf *destination, Charger *charger)
{
    // TODO: stop DC/DC

    destination->voltage_topping                = source->voltage_topping;
    destination->voltage_recharge               = source->voltage_recharge;
    destination->voltage_load_reconnect         = source->voltage_load_reconnect;
    destination->voltage_load_disconnect        = source->voltage_load_disconnect;
    destination->voltage_absolute_max           = source->voltage_absolute_max;
    destination->voltage_absolute_min           = source->voltage_absolute_min;
    destination->charge_current_max             = source->charge_current_max;
    destination->current_cutoff_topping         = source->current_cutoff_topping;
    destination->time_limit_topping             = source->time_limit_topping;
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
        if (charger != NULL) {
            charger->discharged_Ah = 0;
            charger->usable_capacity = 0;
            charger->soh = 0;
        }
    }

    // TODO:
    // - update also DC/DC etc. (now this function only works at system startup)
    // - restart DC/DC
}


void charger_init(Charger *charger)
{
    charger->num_batteries = 1;             // initialize with only one battery in series
    charger->soh = 100;                     // assume new battery
    charger->bat_temperature = 25.0;
    charger->first_full_charge_reached = false;
}

void charger_detect_num_batteries(Charger *charger, BatConf *bat, DcBus *bus)
{
    if (bus->voltage > bat->voltage_absolute_min * 2 &&
        bus->voltage < bat->voltage_absolute_max * 2) {
        charger->num_batteries = 2;
        printf("Detected 24V battery\n");
    }
    else {
        printf("Detected 12V battery\n");
    }
}

void battery_update_soc(BatConf *bat_conf, Charger *charger, DcBus *bus)
{
    static int soc_filtered = 0;       // SOC / 100 for better filtering

    if (fabs(bus->current) < 0.2) {
        int soc_new = (int)((bus->voltage - bat_conf->ocv_empty) /
                   (bat_conf->ocv_full - bat_conf->ocv_empty) * 10000.0);

        if (soc_new > 500 && soc_filtered == 0) {
            // bypass filter during initialization
            soc_filtered = soc_new;
        }
        else {
            // filtering to adjust SOC very slowly
            soc_filtered += (soc_new - soc_filtered) / 100;
        }

        if (soc_filtered > 10000) {
            soc_filtered = 10000;
        }
        else if (soc_filtered < 0) {
            soc_filtered = 0;
        }
        charger->soc = soc_filtered / 100;
    }

    charger->discharged_Ah += -bus->current / 3600.0;   // charged current is positive: change sign
}

static void _enter_state(Charger* charger, int next_state)
{
    //printf("Enter State: %d\n", next_state);
    charger->time_state_changed = time(NULL);
    charger->state = next_state;
}

void charger_state_machine(DcBus *bus, BatConf *bat_conf, Charger *charger)
{
    //printf("time_state_change = %d, time = %d, v_bat = %f, i_bat = %f\n", charger->time_state_changed, time(NULL), voltage, current);

    // load output state is defined by battery bus dis_allowed flag
    if (bus->dis_allowed == true
        && bus->voltage < charger->num_batteries *
        (bat_conf->voltage_load_disconnect - bus->current * bus->dis_droop_res))
    {
        // low state of charge
        bus->dis_allowed = false;
        charger->num_deep_discharges++;

        if (charger->usable_capacity < 0.1) {
            // reset to measured value if discharged the first time
            if (charger->first_full_charge_reached) {
                charger->usable_capacity = charger->discharged_Ah;
            }
        } else {
            // slowly adapt new measurements with low-pass filter
            charger->usable_capacity =
                0.8 * charger->usable_capacity +
                0.2 * charger->discharged_Ah;
        }

        // simple SOH estimation
        charger->soh = charger->usable_capacity / bat_conf->nominal_capacity;
    }
    else if (bus->dis_allowed == true
            && (charger->bat_temperature > bat_conf->discharge_temp_max
            || charger->bat_temperature < bat_conf->discharge_temp_min))
    {
        // temperature limits exceeded
        bus->dis_allowed = false;
    }

    if (bus->voltage >= charger->num_batteries *
        (bat_conf->voltage_load_reconnect - bus->current * bus->dis_droop_res)
        && charger->bat_temperature < bat_conf->discharge_temp_max - 1
        && charger->bat_temperature > bat_conf->discharge_temp_min + 1)
    {
        bus->dis_allowed = true;
    }

    // check battery temperature for charging direction
    if (charger->bat_temperature > bat_conf->charge_temp_max
        || charger->bat_temperature < bat_conf->charge_temp_min)
    {
        bus->chg_allowed = false;
        _enter_state(charger, CHG_STATE_IDLE);
    }

    // state machine
    switch (charger->state) {
        case CHG_STATE_IDLE: {
            if  (bus->voltage < bat_conf->voltage_recharge * charger->num_batteries
                && (time(NULL) - charger->time_state_changed) > bat_conf->time_limit_recharge
                && charger->bat_temperature < bat_conf->charge_temp_max - 1
                && charger->bat_temperature > bat_conf->charge_temp_min + 1)
            {
                bus->chg_voltage_target = charger->num_batteries * (bat_conf->voltage_topping +
                    bat_conf->temperature_compensation * (charger->bat_temperature - 25));
                bus->chg_current_max = bat_conf->charge_current_max;
                bus->chg_allowed = true;
                charger->full = false;
                _enter_state(charger, CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_BULK: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->voltage_topping +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            if (bus->voltage > charger->num_batteries *
                (bus->chg_voltage_target - bus->current * bus->chg_droop_res))
            {
                _enter_state(charger, CHG_STATE_TOPPING);
            }
            break;
        }
        case CHG_STATE_TOPPING: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->voltage_topping +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            if (bus->voltage >= charger->num_batteries *
                (bus->chg_voltage_target - bus->current * bus->chg_droop_res))
            {
                charger->time_voltage_limit_reached = time(NULL);
            }

            // cut-off limit reached because battery full (i.e. CV limit still
            // reached by available solar power within last 2s) or CV period long enough?
            if ((bus->current < bat_conf->current_cutoff_topping &&
                (time(NULL) - charger->time_voltage_limit_reached) < 2)
                || (time(NULL) - charger->time_state_changed) > bat_conf->time_limit_topping)
            {
                charger->full = true;
                charger->num_full_charges++;
                charger->discharged_Ah = 0;         // reset coulomb counter
                charger->first_full_charge_reached = true;

                /*if (bat_conf->equalization_enabled) {
                    // TODO: additional conditions!
                    bus->chg_voltage_target = bat_conf->voltage_equalization;
                    bus->chg_current_max = bat_conf->current_limit_equalization;
                    _enter_state(charger, CHG_STATE_EQUALIZATION);
                }
                else */if (bat_conf->trickle_enabled) {
                    bus->chg_voltage_target = charger->num_batteries * (bat_conf->voltage_trickle +
                        bat_conf->temperature_compensation * (charger->bat_temperature - 25));
                    _enter_state(charger, CHG_STATE_TRICKLE);
                }
                else {
                    bus->chg_current_max = 0;
                    bus->chg_allowed = false;
                    _enter_state(charger, CHG_STATE_IDLE);
                }
            }
            break;
        }
        case CHG_STATE_TRICKLE: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->voltage_trickle +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            if (bus->voltage >= charger->num_batteries *
                (bus->chg_voltage_target - bus->current * bus->chg_droop_res))
            {
                charger->time_voltage_limit_reached = time(NULL);
            }

            if (time(NULL) - charger->time_voltage_limit_reached > bat_conf->time_trickle_recharge)
            {
                bus->chg_current_max = bat_conf->charge_current_max;
                charger->full = false;
                _enter_state(charger, CHG_STATE_BULK);
            }
            // assumption: trickle does not harm the battery --> never go back to idle
            // (for Li-ion battery: disable trickle!)
            break;
        }
    }
}

void battery_init_dc_bus(DcBus *bus, BatConf *bat, unsigned int num_batteries)
{
    unsigned int n = (num_batteries == 2 ? 2 : 1);  // only 1 or 2 allowed

    bus->dis_allowed = true;
    bus->dis_voltage_start = bat->voltage_load_reconnect * n;
    bus->dis_voltage_stop = bat->voltage_load_disconnect * n;
    bus->dis_current_max = -bat->charge_current_max;          // TODO: discharge current

    // negative sign for compensation of actual resistance
    bus->dis_droop_res = -(bat->internal_resistance + -bat->wire_resistance) * n;

    bus->chg_allowed = true;
    bus->chg_voltage_target = bat->voltage_topping * n;
    bus->chg_voltage_min = bat->voltage_absolute_min * n;
    bus->chg_current_max = bat->charge_current_max;

    // negative sign for compensation of actual resistance
    bus->chg_droop_res = -bat->wire_resistance * n;
}
