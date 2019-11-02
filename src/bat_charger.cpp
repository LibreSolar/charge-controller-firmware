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

#include "device_status.h"
extern DeviceStatus dev_stat;

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

void Charger::detect_num_batteries(BatConf *bat)
{
    if (port->voltage > bat->voltage_absolute_min * 2 &&
        port->voltage < bat->voltage_absolute_max * 2) {
        num_batteries = 2;
        printf("Detected two batteries (total %.2f V max)\n", bat->topping_voltage * 2);
    }
    else {
        printf("Detected single battery (%.2f V max)\n", bat->topping_voltage);
    }
}

void Charger::update_soc(BatConf *bat_conf)
{
    static int soc_filtered = 0;       // SOC / 100 for better filtering

    if (fabs(port->current) < 0.2) {
        int soc_new = (int)((port->voltage - bat_conf->ocv_empty) /
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
        soc = soc_filtered / 100;
    }

    discharged_Ah += -port->current / 3600.0;   // charged current is positive: change sign
}

void Charger::enter_state(int next_state)
{
    //printf("Enter State: %d\n", next_state);
    time_state_changed = time(NULL);
    state = next_state;
}

void Charger::discharge_control(BatConf *bat_conf)
{
    // load output state is defined by battery negative current limit
    if (port->neg_current_limit < 0) {
        // discharging currently allowed. see if that's still valid:
        if (port->voltage < num_batteries *
            (bat_conf->voltage_load_disconnect - port->current * port->neg_droop_res))
        {
            // low state of charge
            port->neg_current_limit = 0;
            num_deep_discharges++;
            dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);

            if (usable_capacity < 0.1) {
                // reset to measured value if discharged the first time
                if (first_full_charge_reached) {
                    usable_capacity = discharged_Ah;
                }
            }
            else {
                // slowly adapt new measurements with low-pass filter
                usable_capacity =
                    0.8 * usable_capacity +
                    0.2 * discharged_Ah;
            }

            // simple SOH estimation
            soh = usable_capacity / bat_conf->nominal_capacity;
        }
        else if (bat_temperature > bat_conf->discharge_temp_max) {
            port->neg_current_limit = 0;
            dev_stat.set_error(ERR_BAT_DIS_OVERTEMP);
        }
        else if (bat_temperature < bat_conf->discharge_temp_min) {
            port->neg_current_limit = 0;
            dev_stat.set_error(ERR_BAT_DIS_UNDERTEMP);
        }
    }
    else {
        // discharging currently not allowed. should we allow it?
        if (port->voltage >= num_batteries *
            (bat_conf->voltage_load_reconnect - port->current * port->neg_droop_res)
            && bat_temperature < bat_conf->discharge_temp_max - 1
            && bat_temperature > bat_conf->discharge_temp_min + 1)
        {
            // discharge current is stored as absolute value in bat_conf, but defined
            // as negative current for power port
            port->neg_current_limit = -bat_conf->discharge_current_max;

            // delete all discharge error flags
            dev_stat.clear_error(ERR_BAT_DIS_OVERTEMP);
            dev_stat.clear_error(ERR_BAT_DIS_UNDERTEMP);
            dev_stat.clear_error(ERR_BAT_UNDERVOLTAGE);
        }
    }
}

void Charger::charge_control(BatConf *bat_conf)
{
    // check battery temperature for charging direction
    if (bat_temperature > bat_conf->charge_temp_max) {
        port->pos_current_limit = 0;
        dev_stat.set_error(ERR_BAT_CHG_OVERTEMP);
        enter_state(CHG_STATE_IDLE);
    }
    else if (bat_temperature < bat_conf->charge_temp_min) {
        port->pos_current_limit = 0;
        dev_stat.set_error(ERR_BAT_CHG_UNDERTEMP);
        enter_state(CHG_STATE_IDLE);
    }

    // state machine
    switch (state) {
        case CHG_STATE_IDLE: {
            if  (port->voltage < bat_conf->voltage_recharge * num_batteries
                && (time(NULL) - time_state_changed) > bat_conf->time_limit_recharge
                && bat_temperature < bat_conf->charge_temp_max - 1
                && bat_temperature > bat_conf->charge_temp_min + 1)
            {
                port->sink_voltage_max = num_batteries * (bat_conf->topping_voltage +
                    bat_conf->temperature_compensation * (bat_temperature - 25));
                port->pos_current_limit = bat_conf->charge_current_max;
                full = false;
                dev_stat.clear_error(ERR_BAT_CHG_OVERTEMP);
                dev_stat.clear_error(ERR_BAT_CHG_UNDERTEMP);
                enter_state(CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_BULK: {
            // continuously adjust voltage setting for temperature compensation
            port->sink_voltage_max = num_batteries * (bat_conf->topping_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25));

            if (port->voltage > num_batteries *
                (port->sink_voltage_max - port->current * port->pos_droop_res))
            {
                enter_state(CHG_STATE_TOPPING);
            }
            break;
        }
        case CHG_STATE_TOPPING: {
            // continuously adjust voltage setting for temperature compensation
            port->sink_voltage_max = num_batteries * (bat_conf->topping_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25));

            if (port->voltage >= num_batteries *
                (port->sink_voltage_max - port->current * port->pos_droop_res))
            {
                time_voltage_limit_reached = time(NULL);
            }

            // cut-off limit reached because battery full (i.e. CV limit still
            // reached by available solar power within last 2s) or CV period long enough?
            if ((port->current < bat_conf->topping_current_cutoff &&
                (time(NULL) - time_voltage_limit_reached) < 2)
                || (time(NULL) - time_state_changed) > bat_conf->topping_duration)
            {
                full = true;
                num_full_charges++;
                discharged_Ah = 0;         // reset coulomb counter
                first_full_charge_reached = true;

                if (bat_conf->equalization_enabled && (
                    (time(NULL) - time_last_equalization) / (24*60*60)
                    >= bat_conf->equalization_trigger_days ||
                    num_deep_discharges - deep_dis_last_equalization
                    >= bat_conf->equalization_trigger_deep_cycles))
                {
                    port->sink_voltage_max = bat_conf->equalization_voltage;
                    port->pos_current_limit = bat_conf->equalization_current_limit;
                    enter_state(CHG_STATE_EQUALIZATION);
                }
                else if (bat_conf->trickle_enabled) {
                    port->sink_voltage_max = num_batteries * (bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25));
                    enter_state(CHG_STATE_TRICKLE);
                }
                else {
                    port->pos_current_limit = 0;
                    enter_state(CHG_STATE_IDLE);
                }
            }
            break;
        }
        case CHG_STATE_TRICKLE: {
            // continuously adjust voltage setting for temperature compensation
            port->sink_voltage_max = num_batteries * (bat_conf->trickle_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25));

            if (port->voltage >= num_batteries *
                (port->sink_voltage_max - port->current * port->pos_droop_res))
            {
                time_voltage_limit_reached = time(NULL);
            }

            if (time(NULL) - time_voltage_limit_reached > bat_conf->trickle_recharge_time)
            {
                port->pos_current_limit = bat_conf->charge_current_max;
                full = false;
                enter_state(CHG_STATE_BULK);
            }
            // assumption: trickle does not harm the battery --> never go back to idle
            // (for Li-ion battery: disable trickle!)
            break;
        }
        case CHG_STATE_EQUALIZATION: {
            // continuously adjust voltage setting for temperature compensation
            port->sink_voltage_max = num_batteries * (bat_conf->equalization_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25));

            // current or time limit for equalization reached
            if (time(NULL) - time_state_changed > bat_conf->equalization_duration)
            {
                // reset triggers
                time_last_equalization = time(NULL);
                deep_dis_last_equalization = num_deep_discharges;

                discharged_Ah = 0;         // reset coulomb counter again

                if (bat_conf->trickle_enabled) {
                    port->sink_voltage_max = num_batteries * (bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25));
                    enter_state(CHG_STATE_TRICKLE);
                }
                else {
                    port->pos_current_limit = 0;
                    enter_state(CHG_STATE_IDLE);
                }
            }
            break;
        }
    }
}

void battery_init_dc_bus(PowerPort *port, BatConf *bat, unsigned int num_batteries)
{
    unsigned int n = (num_batteries == 2 ? 2 : 1);  // only 1 or 2 allowed

    port->src_voltage_start = bat->voltage_load_reconnect * n;
    port->src_voltage_stop = bat->voltage_load_disconnect * n;
    port->neg_current_limit = -bat->discharge_current_max;

    // negative sign for compensation of actual resistance
    port->neg_droop_res = -(bat->internal_resistance + -bat->wire_resistance) * n;

    port->sink_voltage_max = bat->topping_voltage * n;
    port->sink_voltage_min = bat->voltage_absolute_min * n;
    port->pos_current_limit = bat->charge_current_max;

    // negative sign for compensation of actual resistance
    port->pos_droop_res = -bat->wire_resistance * n;
}
