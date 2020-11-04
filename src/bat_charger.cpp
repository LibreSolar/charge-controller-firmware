/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bat_charger.h"

#include <zephyr.h>

#ifndef UNIT_TEST
#include <logging/log.h>
LOG_MODULE_REGISTER(bat_charger, CONFIG_LOG_DEFAULT_LEVEL);
#endif

#include <math.h>       // for fabs function
#include <stdio.h>

#include "helper.h"
#include "device_status.h"

extern DeviceStatus dev_stat;
extern LoadOutput load;

// DISCHARGE_CURRENT_MAX used to estimate current-compensation of load disconnect voltage
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
#define DISCHARGE_CURRENT_MAX DT_PROP(DT_CHILD(DT_PATH(outputs), load), current_max)
#else
#define DISCHARGE_CURRENT_MAX DT_PROP(DT_PATH(dcdc), current_max)
#endif

void battery_conf_init(BatConf *bat, int type, int num_cells, float nominal_capacity)
{
    bat->nominal_capacity = nominal_capacity;

    // 1C should be safe for all batteries
    bat->charge_current_max    = bat->nominal_capacity;
    bat->discharge_current_max = bat->nominal_capacity;

    bat->time_limit_recharge = 60;              // sec
    bat->topping_duration    = 120*60;          // sec

    bat->charge_temp_max    =  50;
    bat->charge_temp_min    = -10;
    bat->discharge_temp_max =  50;
    bat->discharge_temp_min = -10;

    switch (type)
    {
        case BAT_TYPE_FLOODED:
        case BAT_TYPE_AGM:
        case BAT_TYPE_GEL:
            bat->voltage_absolute_max    = static_cast<float>(num_cells) * 2.45F;
            bat->topping_voltage         = static_cast<float>(num_cells) * 2.4F;
            bat->voltage_recharge        = static_cast<float>(num_cells) * 2.3F;

            // Cell-level thresholds based on EN 62509:2011 (both thresholds current-compensated)
            bat->voltage_load_disconnect = static_cast<float>(num_cells) * 1.95F;
            bat->voltage_load_reconnect  = static_cast<float>(num_cells) * 2.10F;

            // assumption: Battery selection matching charge controller
            bat->internal_resistance     = static_cast<float>(num_cells) * (1.95F - 1.80F) /
                DISCHARGE_CURRENT_MAX;

            bat->voltage_absolute_min    = static_cast<float>(num_cells) * 1.6F;

            // Voltages during idle (no charging/discharging current)
            bat->ocv_full                = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_FLOODED) ? 2.10F : 2.15F);
            bat->ocv_empty               = static_cast<float>(num_cells) * 1.90F;

            // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
            bat->topping_current_cutoff  = bat->nominal_capacity * 0.04F;  // 3-5 % of C/1

            bat->trickle_enabled         = true;
            bat->trickle_recharge_time   = 30*60;
            // Values as suggested in EN 62509:2011
            bat->trickle_voltage         = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_FLOODED) ? 2.35F : 2.3F);

            // Enable for flooded batteries only, according to
            // https://discoverbattery.com/battery-101/equalizing-flooded-batteries-only
            bat->equalization_enabled    = false;
            // Values as suggested in EN 62509:2011
            bat->equalization_voltage    = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_FLOODED) ? 2.50F : 2.45F);
            bat->equalization_duration   = 60*60;
            bat->equalization_current_limit = (1.0F / 7.0F) * bat->nominal_capacity;
            bat->equalization_trigger_days = 60;
            bat->equalization_trigger_deep_cycles = 10;

            bat->temperature_compensation = -0.003F;     // -3 mV/°C/cell
            break;

        case BAT_TYPE_LFP:
            bat->voltage_absolute_max    = static_cast<float>(num_cells) * 3.60F;
            bat->topping_voltage         = static_cast<float>(num_cells) * 3.55F;    // CV voltage
            bat->voltage_recharge        = static_cast<float>(num_cells) * 3.35F;

            bat->voltage_load_disconnect = static_cast<float>(num_cells) * 3.00F;
            bat->voltage_load_reconnect  = static_cast<float>(num_cells) * 3.15F;

            // 5% voltage drop at max current
            bat->internal_resistance    = bat->voltage_load_disconnect * 0.05F /
                                          DISCHARGE_CURRENT_MAX;
            bat->voltage_absolute_min   = static_cast<float>(num_cells) * 2.0F;

            // will give really nonlinear SOC calculation because of flat OCV of LFP cells...
            bat->ocv_full   = static_cast<float>(num_cells) * 3.4F;
            bat->ocv_empty  = static_cast<float>(num_cells) * 3.0F;

            // C/10 cut-off at end of CV phase by default
            bat->topping_current_cutoff = bat->nominal_capacity / 10;

            bat->trickle_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_NMC:
        case BAT_TYPE_NMC_HV:
            bat->topping_voltage         = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_NMC_HV) ? 4.35F : 4.20F);
            bat->voltage_absolute_max    = bat->topping_voltage +
                static_cast<float>(num_cells) * 0.05F;
            bat->voltage_recharge        = static_cast<float>(num_cells) * 3.9F;

            bat->voltage_load_disconnect = static_cast<float>(num_cells) * 3.3F;
            bat->voltage_load_reconnect  = static_cast<float>(num_cells) * 3.6F;

            // 5% voltage drop at max current
            bat->internal_resistance     = bat->voltage_load_disconnect * 0.05F /
                DISCHARGE_CURRENT_MAX;

            bat->voltage_absolute_min    = static_cast<float>(num_cells) * 2.5F;

            bat->ocv_full                = static_cast<float>(num_cells) * 4.0F;
            bat->ocv_empty               = static_cast<float>(num_cells) * 3.0F;

            // C/10 cut-off at end of CV phase by default
            bat->topping_current_cutoff  = bat->nominal_capacity / 10;

            bat->trickle_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_CUSTOM:
            bat->voltage_absolute_max = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_ABS_MAX_VOLTAGE_MV);

            bat->topping_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_TOPPING_VOLTAGE_MV);

            bat->voltage_recharge = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_RECHARGE_VOLTAGE_MV);

            bat->voltage_load_disconnect = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_DISCONNECT_VOLTAGE_MV);
            bat->voltage_load_reconnect = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_RECONNECT_VOLTAGE_MV);

            bat->internal_resistance = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_INTERNAL_RESISTANCE_MOHM);

            bat->voltage_absolute_min = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_ABS_MIN_VOLTAGE_MV);

            // Voltages during idle (no charging/discharging current)
            bat->ocv_full = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_OCV_FULL_MV);
            bat->ocv_empty = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_OCV_EMPTY_MV);

            // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
            bat->topping_current_cutoff = bat->nominal_capacity * 0.04F;  // 3-5 % of C/1

            bat->trickle_enabled = IS_ENABLED(CONFIG_CELL_TRICKLE);
            bat->trickle_recharge_time = CONFIG_CELL_TRICKLE_RECHARGE_TIME;
            bat->trickle_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_TRICKLE_VOLTAGE_MV);

            bat->equalization_enabled = IS_ENABLED(CONFIG_CELL_EQUALIZATION);
            bat->equalization_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_EQUALIZATION_VOLTAGE_MV);
            bat->equalization_duration = CONFIG_CELL_EQUALIZATION_DURATION;
            bat->equalization_current_limit = (1.0F / 7.0F) * bat->nominal_capacity;
            bat->equalization_trigger_days = CONFIG_CELL_EQUALIZATION_TRIGGER_DAYS;
            bat->equalization_trigger_deep_cycles = CONFIG_CELL_EQUALIZATION_TRIGGER_DEEP_CYCLES;

            bat->temperature_compensation = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_TEMP_COMPENSATION_MV_K);

            bat->charge_temp_max    = CONFIG_BAT_CHARGE_TEMP_MAX;
            bat->charge_temp_min    = CONFIG_BAT_CHARGE_TEMP_MIN;
            bat->discharge_temp_max = CONFIG_BAT_DISCHARGE_TEMP_MAX;
            bat->discharge_temp_min = CONFIG_BAT_DISCHARGE_TEMP_MIN;
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

    LOG_DBG("battery_conf_check: %d %d %d %d %d %d %d",
        bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.6),
        bat_conf->voltage_recharge < (bat_conf->topping_voltage - 0.4),
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1),
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4),
        bat_conf->topping_current_cutoff < (bat_conf->nominal_capacity / 10.0),
        bat_conf->topping_current_cutoff > 0.01,
        (bat_conf->trickle_enabled == false ||
            (bat_conf->trickle_voltage < bat_conf->topping_voltage &&
             bat_conf->trickle_voltage > bat_conf->voltage_load_disconnect))
    );

    return (
        bat_conf->voltage_load_reconnect > (bat_conf->voltage_load_disconnect + 0.4) &&
        bat_conf->voltage_recharge < (bat_conf->topping_voltage - 0.4) &&
        bat_conf->voltage_recharge > (bat_conf->voltage_load_disconnect + 1) &&
        bat_conf->voltage_load_disconnect > (bat_conf->voltage_absolute_min + 0.4) &&
        // max. 10% drop
        bat_conf->internal_resistance < bat_conf->voltage_load_disconnect * 0.1 /
            DISCHARGE_CURRENT_MAX &&
        // max. 3% loss
        bat_conf->wire_resistance < bat_conf->topping_voltage * 0.03 / DISCHARGE_CURRENT_MAX &&
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
    destination->topping_duration               = source->topping_duration;
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

bool battery_conf_changed(BatConf *a, BatConf *b)
{
    return (
        a->topping_voltage                != b->topping_voltage ||
        a->voltage_recharge               != b->voltage_recharge ||
        a->voltage_load_reconnect         != b->voltage_load_reconnect ||
        a->voltage_load_disconnect        != b->voltage_load_disconnect ||
        a->voltage_absolute_max           != b->voltage_absolute_max ||
        a->voltage_absolute_min           != b->voltage_absolute_min ||
        a->charge_current_max             != b->charge_current_max ||
        a->topping_current_cutoff         != b->topping_current_cutoff ||
        a->topping_duration               != b->topping_duration ||
        a->trickle_enabled                != b->trickle_enabled ||
        a->trickle_voltage                != b->trickle_voltage ||
        a->trickle_recharge_time          != b->trickle_recharge_time ||
        a->charge_temp_max                != b->charge_temp_max ||
        a->charge_temp_min                != b->charge_temp_min ||
        a->discharge_temp_max             != b->discharge_temp_max ||
        a->discharge_temp_min             != b->discharge_temp_min ||
        a->temperature_compensation       != b->temperature_compensation ||
        a->internal_resistance            != b->internal_resistance ||
        a->wire_resistance                != b->wire_resistance
    );
}

void Charger::detect_num_batteries(BatConf *bat) const
{
    if (port->bus->voltage > bat->voltage_absolute_min * 2 &&
        port->bus->voltage < bat->voltage_absolute_max * 2)
    {
        port->bus->series_multiplier = 2;
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
        int soc_new = (int)((port->bus->voltage - bat_conf->ocv_empty) /
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

    discharged_Ah += -port->current / 3600.0F;   // charged current is positive: change sign
}

void Charger::enter_state(int next_state)
{
    LOG_DBG("Enter State: %d", next_state);
    time_state_changed = uptime();
    state = next_state;
}

void Charger::discharge_control(BatConf *bat_conf)
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load)) || \
    DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))

    if (!empty) {
        // as we don't have a proper SOC estimation, we determine an empty battery by the main
        // load output being switched off
        if (flags_check(&load.error_flags, ERR_LOAD_SHEDDING)) {
            empty = true;
            num_deep_discharges++;

            if (usable_capacity == 0.0F) {
                // reset to measured value if discharged the first time
                usable_capacity = discharged_Ah;
            }
            else {
                // slowly adapt new measurements with low-pass filter
                usable_capacity =
                    0.8F * usable_capacity +
                    0.2F * discharged_Ah;
            }

            // simple SOH estimation
            soh = usable_capacity / bat_conf->nominal_capacity;
        }
    }
    else {
        if (!flags_check(&load.error_flags, ERR_LOAD_SHEDDING)) {
            empty = false;
        }
    }

    // negative current limit = allowed battery discharge current
    if (port->neg_current_limit < 0) {

        // This limit should normally never be reached, as the load output settings should be
        // higher. The flag can be used to trigger actions of last resort, e.g. deep-sleep
        // of the charge controller itself.
        if (port->bus->voltage < port->bus->src_control_voltage(bat_conf->voltage_absolute_min)) {
            port->neg_current_limit = 0;
            dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);
        }

        if (bat_temperature > bat_conf->discharge_temp_max) {
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

        if (port->bus->voltage >=
            port->bus->src_control_voltage(bat_conf->voltage_absolute_min + 0.1F))
        {
            dev_stat.clear_error(ERR_BAT_UNDERVOLTAGE);
        }

        if (bat_temperature < bat_conf->discharge_temp_max - 1 &&
            bat_temperature > bat_conf->discharge_temp_min + 1)
        {
            dev_stat.clear_error(ERR_BAT_DIS_OVERTEMP | ERR_BAT_DIS_UNDERTEMP);
        }

        if (!dev_stat.has_error(
            ERR_BAT_UNDERVOLTAGE | ERR_BAT_DIS_OVERTEMP | ERR_BAT_DIS_UNDERTEMP))
        {
            // discharge current is stored as absolute value in bat_conf, but defined
            // as negative current for power port
            port->neg_current_limit = -bat_conf->discharge_current_max;

        }
    }
#endif
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

    if (dev_stat.has_error(ERR_BAT_OVERVOLTAGE) &&
        port->bus->voltage < (bat_conf->voltage_absolute_max - 0.5F) * port->bus->series_multiplier)
    {
        dev_stat.clear_error(ERR_BAT_OVERVOLTAGE);
    }

    // state machine
    switch (state) {
        case CHG_STATE_IDLE: {
            if  (port->bus->voltage < port->bus->sink_control_voltage(bat_conf->voltage_recharge)
                && port->bus->voltage > port->bus->sink_control_voltage(bat_conf->voltage_absolute_min) // ToDo set error flag otherwise
                && (uptime() - time_state_changed) > bat_conf->time_limit_recharge
                && bat_temperature < bat_conf->charge_temp_max - 1
                && bat_temperature > bat_conf->charge_temp_min + 1)
            {
                port->bus->sink_voltage_intercept = bat_conf->topping_voltage +
                    bat_conf->temperature_compensation * (bat_temperature - 25);
                port->pos_current_limit = bat_conf->charge_current_max;
                full = false;
                dev_stat.clear_error(ERR_BAT_CHG_OVERTEMP);
                dev_stat.clear_error(ERR_BAT_CHG_UNDERTEMP);
                dev_stat.clear_error(ERR_BAT_OVERVOLTAGE);
                enter_state(CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_BULK: {
            // continuously adjust voltage setting for temperature compensation
            port->bus->sink_voltage_intercept = bat_conf->topping_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            if (port->bus->voltage > port->bus->sink_control_voltage()) {
                target_voltage_timer = 0;
                enter_state(CHG_STATE_TOPPING);
            }
            break;
        }
        case CHG_STATE_TOPPING: {
            // continuously adjust voltage setting for temperature compensation
            port->bus->sink_voltage_intercept = bat_conf->topping_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            if (port->bus->voltage >= port->bus->sink_control_voltage() - 0.05F) {
                // battery is full if topping target voltage is still reached (i.e. sufficient
                // solar power available) and time limit or cut-off current reached
                if (port->current < bat_conf->topping_current_cutoff ||
                    target_voltage_timer > bat_conf->topping_duration)
                {
                    full = true;
                }
                target_voltage_timer++;
            }
            else if (uptime() - time_state_changed > 8 * 60 * 60) {
                // in topping phase already for 8 hours (i.e. not enough solar power available)
                // --> go back to bulk charging for the next day
                enter_state(CHG_STATE_BULK);
            }

            if (full) {
                num_full_charges++;
                discharged_Ah = 0;         // reset coulomb counter

                if (bat_conf->equalization_enabled && (
                    (uptime() - time_last_equalization) / (24*60*60)
                    >= bat_conf->equalization_trigger_days ||
                    num_deep_discharges - deep_dis_last_equalization
                    >= bat_conf->equalization_trigger_deep_cycles))
                {
                    port->bus->sink_voltage_intercept = bat_conf->equalization_voltage;
                    port->pos_current_limit = bat_conf->equalization_current_limit;
                    enter_state(CHG_STATE_EQUALIZATION);
                }
                else if (bat_conf->trickle_enabled) {
                    port->bus->sink_voltage_intercept = bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25);
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
            port->bus->sink_voltage_intercept = bat_conf->trickle_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            if (port->bus->voltage >= port->bus->sink_control_voltage()) {
                time_target_voltage_reached = uptime();
            }

            if (uptime() - time_target_voltage_reached > bat_conf->trickle_recharge_time)
            {
                // the battery was discharged: trickle voltage could not be reached anymore
                port->pos_current_limit = bat_conf->charge_current_max;
                full = false;
                // assumption: trickle does not harm the battery --> never go back to idle
                // (for Li-ion battery: disable trickle!)
                enter_state(CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_EQUALIZATION: {
            // continuously adjust voltage setting for temperature compensation
            port->bus->sink_voltage_intercept = bat_conf->equalization_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            // current or time limit for equalization reached
            if (uptime() - time_state_changed > bat_conf->equalization_duration)
            {
                // reset triggers
                time_last_equalization = uptime();
                deep_dis_last_equalization = num_deep_discharges;

                discharged_Ah = 0;         // reset coulomb counter again

                if (bat_conf->trickle_enabled) {
                    port->bus->sink_voltage_intercept = bat_conf->trickle_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25);
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

void Charger::init_terminal(BatConf *bat) const
{
    port->bus->sink_voltage_intercept = bat->topping_voltage;
    port->bus->src_voltage_intercept = bat->voltage_load_disconnect;

    port->neg_current_limit = -bat->discharge_current_max;
    port->pos_current_limit = bat->charge_current_max;

    /*
     * Negative sign for compensation of actual resistance
     *
     * droop_res is multiplied with number of series connected batteries to calculate control
     * voltage, so we need to divide by number of batteries here for correction
    */
    port->bus->sink_droop_res = -bat->wire_resistance /
        static_cast<float>(port->bus->series_multiplier);

    /*
     * In discharging direction also include battery internal resistance for current-compensation
     * of voltage setpoints
     */
    port->bus->src_droop_res = -bat->wire_resistance /
        static_cast<float>(port->bus->series_multiplier)
        - bat->internal_resistance;
}
