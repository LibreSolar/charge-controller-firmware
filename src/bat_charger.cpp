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

    // 1C should be safe for all batteries
    bat->charge_current_max = bat->nominal_capacity;
    bat->discharge_current_max = bat->nominal_capacity;

    bat->time_limit_recharge = 60;              // sec
    bat->topping_duration = 120*60;                // sec

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
            bat->topping_voltage = num_cells * 2.4;
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
            bat->topping_current_cutoff = bat->nominal_capacity * 0.04;  // 3-5 % of C/1

            bat->trickle_enabled = true;
            bat->trickle_recharge_time = 30*60;
            // Values as suggested in EN 62509:2011
            bat->trickle_voltage = num_cells * ((type == BAT_TYPE_FLOODED) ? 2.35 : 2.3);

            // Enable for flooded batteries only, according to
            // https://discoverbattery.com/battery-101/equalizing-flooded-batteries-only
            bat->equalization_enabled = false;
            // Values as suggested in EN 62509:2011
            bat->equalization_voltage = num_cells * ((type == BAT_TYPE_FLOODED) ? 2.50 : 2.45);
            bat->equalization_duration = 60*60;
            bat->equalization_current_limit = (1.0 / 7.0) * bat->nominal_capacity;
            bat->equalization_trigger_days = 60;
            bat->equalization_trigger_deep_cycles = 10;

            bat->temperature_compensation = -0.003;     // -3 mV/°C/cell
            break;

        case BAT_TYPE_LFP:
            bat->voltage_absolute_max = num_cells * 3.60;
            bat->topping_voltage = num_cells * 3.55;               // CV voltage
            bat->voltage_recharge = num_cells * 3.35;

            bat->voltage_load_disconnect = num_cells * 3.0;
            bat->voltage_load_reconnect  = num_cells * 3.15;

            // 5% voltage drop at max current
            bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;
            bat->voltage_absolute_min = num_cells * 2.0;

            bat->ocv_full = num_cells * 3.4;       // will give really nonlinear SOC calculation
            bat->ocv_empty = num_cells * 3.0;      // because of flat OCV of LFP cells...

            // C/10 cut-off at end of CV phase by default
            bat->topping_current_cutoff = bat->nominal_capacity / 10;

            bat->trickle_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0.0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_NMC:
        case BAT_TYPE_NMC_HV:
            bat->topping_voltage = num_cells * ((type == BAT_TYPE_NMC_HV) ? 4.35 : 4.20);
            bat->voltage_absolute_max = num_cells * (bat->topping_voltage + 0.05);
            bat->voltage_recharge = num_cells * 3.9;

            bat->voltage_load_disconnect = num_cells * 3.3;
            bat->voltage_load_reconnect  = num_cells * 3.6;

            // 5% voltage drop at max current
            bat->internal_resistance = bat->voltage_load_disconnect * 0.05 / LOAD_CURRENT_MAX;

            bat->voltage_absolute_min = num_cells * 2.5;

            bat->ocv_full = num_cells * 4.0;
            bat->ocv_empty = num_cells * 3.0;

            // C/10 cut-off at end of CV phase by default
            bat->topping_current_cutoff = bat->nominal_capacity / 10;

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
            (bat_conf->trickle_voltage < bat_conf->voltage_max &&
             bat_conf->trickle_voltage > bat_conf->voltage_load_disconnect))
    );
    */

    return
       (bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.4) &&
        bat_conf->voltage_recharge < (bat_conf->topping_voltage - 0.4) &&
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1) &&
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4) &&
        // max. 10% drop
        bat_conf->internal_resistance < bat_conf->voltage_load_disconnect * 0.1 / LOAD_CURRENT_MAX &&
        // max. 3% loss
        bat_conf->wire_resistance < bat_conf->topping_voltage * 0.03 / LOAD_CURRENT_MAX &&
        // C/10 or lower current cutoff allowed
        bat_conf->topping_current_cutoff < (bat_conf->nominal_capacity / 10.0) &&
        bat_conf->topping_current_cutoff > 0.01 &&
        (bat_conf->trickle_enabled == false ||
            (bat_conf->trickle_voltage < bat_conf->topping_voltage &&
             bat_conf->trickle_voltage > bat_conf->voltage_load_disconnect))
       );
}

void battery_conf_overwrite(BatConf *source, BatConf *destination, Charger *charger)
{
    // TODO: stop DC/DC

    destination->topping_voltage                = source->topping_voltage;
    destination->voltage_recharge               = source->voltage_recharge;
    destination->voltage_load_reconnect         = source->voltage_load_reconnect;
    destination->voltage_load_disconnect        = source->voltage_load_disconnect;
    destination->voltage_absolute_max           = source->voltage_absolute_max;
    destination->voltage_absolute_min           = source->voltage_absolute_min;
    destination->charge_current_max             = source->charge_current_max;
    destination->topping_current_cutoff         = source->topping_current_cutoff;
    destination->topping_duration             = source->topping_duration;
    destination->trickle_enabled                = source->trickle_enabled;
    destination->trickle_voltage                = source->trickle_voltage;
    destination->trickle_recharge_time          = source->trickle_recharge_time;
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
        printf("Detected two batteries (total %.2f V max)\n", bat->topping_voltage * 2);
    }
    else {
        printf("Detected single battery (%.2f V max)\n", bat->topping_voltage);
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

void battery_discharge_control(DcBus *bus, BatConf *bat_conf, Charger *charger)
{
    // load output state is defined by battery bus dis_allowed flag
    if (bus->dis_current_limit < 0      // discharging allowed
        && bus->voltage < charger->num_batteries *
        (bat_conf->voltage_load_disconnect - bus->current * bus->dis_droop_res))
    {
        // low state of charge
        bus->dis_current_limit = 0;
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
    else if (bus->dis_current_limit < 0
            && (charger->bat_temperature > bat_conf->discharge_temp_max
            || charger->bat_temperature < bat_conf->discharge_temp_min))
    {
        // temperature limits exceeded
        bus->dis_current_limit = 0;
    }

    if (bus->voltage >= charger->num_batteries *
        (bat_conf->voltage_load_reconnect - bus->current * bus->dis_droop_res)
        && charger->bat_temperature < bat_conf->discharge_temp_max - 1
        && charger->bat_temperature > bat_conf->discharge_temp_min + 1)
    {
        // discharge current is stored as absolute value in bat_conf, but defined
        // as negative current for dc_bus
        bus->dis_current_limit = -bat_conf->discharge_current_max;
    }
}

void battery_charge_control(DcBus *bus, BatConf *bat_conf, Charger *charger)
{
    // check battery temperature for charging direction
    if (charger->bat_temperature > bat_conf->charge_temp_max
        || charger->bat_temperature < bat_conf->charge_temp_min)
    {
        bus->chg_current_limit = 0;
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
                bus->chg_voltage_target = charger->num_batteries * (bat_conf->topping_voltage +
                    bat_conf->temperature_compensation * (charger->bat_temperature - 25));
                bus->chg_current_limit = bat_conf->charge_current_max;
                charger->full = false;
                _enter_state(charger, CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_BULK: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->topping_voltage +
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
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->topping_voltage +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            if (bus->voltage >= charger->num_batteries *
                (bus->chg_voltage_target - bus->current * bus->chg_droop_res))
            {
                charger->time_voltage_limit_reached = time(NULL);
            }

            // cut-off limit reached because battery full (i.e. CV limit still
            // reached by available solar power within last 2s) or CV period long enough?
            if ((bus->current < bat_conf->topping_current_cutoff &&
                (time(NULL) - charger->time_voltage_limit_reached) < 2)
                || (time(NULL) - charger->time_state_changed) > bat_conf->topping_duration)
            {
                charger->full = true;
                charger->num_full_charges++;
                charger->discharged_Ah = 0;         // reset coulomb counter
                charger->first_full_charge_reached = true;

                if (bat_conf->equalization_enabled && (
                    (time(NULL) - charger->time_last_equalization) / (24*60*60)
                    >= bat_conf->equalization_trigger_days ||
                    charger->num_deep_discharges - charger->deep_dis_last_equalization
                    >= bat_conf->equalization_trigger_deep_cycles))
                {
                    bus->chg_voltage_target = bat_conf->equalization_voltage;
                    bus->chg_current_limit = bat_conf->equalization_current_limit;
                    _enter_state(charger, CHG_STATE_EQUALIZATION);
                }
                else if (bat_conf->trickle_enabled) {
                    bus->chg_voltage_target = charger->num_batteries * (bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (charger->bat_temperature - 25));
                    _enter_state(charger, CHG_STATE_TRICKLE);
                }
                else {
                    bus->chg_current_limit = 0;
                    _enter_state(charger, CHG_STATE_IDLE);
                }
            }
            break;
        }
        case CHG_STATE_TRICKLE: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->trickle_voltage +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            if (bus->voltage >= charger->num_batteries *
                (bus->chg_voltage_target - bus->current * bus->chg_droop_res))
            {
                charger->time_voltage_limit_reached = time(NULL);
            }

            if (time(NULL) - charger->time_voltage_limit_reached > bat_conf->trickle_recharge_time)
            {
                bus->chg_current_limit = bat_conf->charge_current_max;
                charger->full = false;
                _enter_state(charger, CHG_STATE_BULK);
            }
            // assumption: trickle does not harm the battery --> never go back to idle
            // (for Li-ion battery: disable trickle!)
            break;
        }
        case CHG_STATE_EQUALIZATION: {
            // continuously adjust voltage setting for temperature compensation
            bus->chg_voltage_target = charger->num_batteries * (bat_conf->equalization_voltage +
                bat_conf->temperature_compensation * (charger->bat_temperature - 25));

            // current or time limit for equalization reached
            if (time(NULL) - charger->time_state_changed > bat_conf->equalization_duration)
            {
                // reset triggers
                charger->time_last_equalization = time(NULL);
                charger->deep_dis_last_equalization = charger->num_deep_discharges;

                charger->discharged_Ah = 0;         // reset coulomb counter again

                if (bat_conf->trickle_enabled) {
                    bus->chg_voltage_target = charger->num_batteries * (bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (charger->bat_temperature - 25));
                    _enter_state(charger, CHG_STATE_TRICKLE);
                }
                else {
                    bus->chg_current_limit = 0;
                    _enter_state(charger, CHG_STATE_IDLE);
                }
            }
            break;
        }
    }
}

void battery_init_dc_bus(DcBus *bus, BatConf *bat, unsigned int num_batteries)
{
    unsigned int n = (num_batteries == 2 ? 2 : 1);  // only 1 or 2 allowed

    bus->dis_voltage_start = bat->voltage_load_reconnect * n;
    bus->dis_voltage_stop = bat->voltage_load_disconnect * n;
    bus->dis_current_limit = -bat->discharge_current_max;

    // negative sign for compensation of actual resistance
    bus->dis_droop_res = -(bat->internal_resistance + -bat->wire_resistance) * n;

    bus->chg_voltage_target = bat->topping_voltage * n;
    bus->chg_voltage_min = bat->voltage_absolute_min * n;
    bus->chg_current_limit = bat->charge_current_max;

    // negative sign for compensation of actual resistance
    bus->chg_droop_res = -bat->wire_resistance * n;
}

void charger_update_junction_bus(DcBus *junction, DcBus *bat, DcBus *load)
{
    // relevant for DC/DC
    junction->chg_current_limit = bat->chg_current_limit - bat->current + load->current;
    junction->chg_voltage_target = bat->chg_voltage_target;

    // relevant for load output
    junction->dis_voltage_start = bat->dis_voltage_start;
    junction->dis_voltage_stop = bat->dis_voltage_stop;

    // this is a bit tricky... in theory, we could add the available DC/DC current to keep the
    // load switched on as long as solar power is available, even if the battery is empty. This
    // needs a fast point-of-load (PoL) control of the DC/DC, which is not possible (yet).
    junction->dis_current_limit = bat->dis_current_limit - bat->current;
}
