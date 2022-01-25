/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bat_charger.h"

#include <zephyr.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(bat_charger, CONFIG_BAT_LOG_LEVEL);

#include <math.h>       // for fabs function
#include <stdio.h>
#include <functional>

#include "board.h"
#include "helper.h"
#include "device_status.h"

extern DeviceStatus dev_stat;
extern LoadOutput load;

// DISCHARGE_CURRENT_MAX used to estimate current-compensation of load disconnect voltage
#if BOARD_HAS_LOAD_OUTPUT
#define DISCHARGE_CURRENT_MAX DT_PROP(DT_CHILD(DT_PATH(outputs), load), current_max)
#else
#define DISCHARGE_CURRENT_MAX DT_PROP(DT_PATH(pcb), dcdc_current_max)
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
            bat->absolute_max_voltage    = static_cast<float>(num_cells) * 2.45F;
            bat->topping_voltage         = static_cast<float>(num_cells) * 2.4F;
            bat->recharge_voltage        = static_cast<float>(num_cells) * 2.2F;

            // Cell-level thresholds based on EN 62509:2011 (both thresholds current-compensated)
            bat->load_disconnect_voltage = static_cast<float>(num_cells) * 1.95F;
            bat->load_reconnect_voltage  = static_cast<float>(num_cells) * 2.10F;

            // assumption: Battery selection matching charge controller
            bat->internal_resistance     = static_cast<float>(num_cells) * (1.95F - 1.80F) /
                DISCHARGE_CURRENT_MAX;

            bat->absolute_min_voltage    = static_cast<float>(num_cells) * 1.6F;

            // Voltages during idle (no charging/discharging current)
            bat->ocv_full                = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_FLOODED) ? 2.10F : 2.15F);
            bat->ocv_empty               = static_cast<float>(num_cells) * 1.90F;

            // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
            bat->topping_cutoff_current  = bat->nominal_capacity * 0.04F;  // 3-5 % of C/1

            bat->float_enabled         = true;
            bat->float_recharge_time   = 30*60;
            // Values as suggested in EN 62509:2011
            bat->float_voltage         = static_cast<float>(num_cells) *
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
            bat->absolute_max_voltage    = static_cast<float>(num_cells) * 3.60F;
            bat->topping_voltage         = static_cast<float>(num_cells) * 3.55F;    // CV voltage
            bat->recharge_voltage        = static_cast<float>(num_cells) * 3.35F;

            bat->load_disconnect_voltage = static_cast<float>(num_cells) * 3.00F;
            bat->load_reconnect_voltage  = static_cast<float>(num_cells) * 3.15F;

            // 5% voltage drop at max current
            bat->internal_resistance    = bat->load_disconnect_voltage * 0.05F /
                                          DISCHARGE_CURRENT_MAX;
            bat->absolute_min_voltage   = static_cast<float>(num_cells) * 2.0F;

            // will give really nonlinear SOC calculation because of flat OCV of LFP cells...
            bat->ocv_full   = static_cast<float>(num_cells) * 3.4F;
            bat->ocv_empty  = static_cast<float>(num_cells) * 3.0F;

            // C/10 cut-off at end of CV phase by default
            bat->topping_cutoff_current = bat->nominal_capacity / 10;

            bat->float_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_NMC:
        case BAT_TYPE_NMC_HV:
            bat->topping_voltage         = static_cast<float>(num_cells) *
                ((type == BAT_TYPE_NMC_HV) ? 4.35F : 4.20F);
            bat->absolute_max_voltage    = bat->topping_voltage +
                static_cast<float>(num_cells) * 0.05F;
            bat->recharge_voltage        = static_cast<float>(num_cells) * 3.9F;

            bat->load_disconnect_voltage = static_cast<float>(num_cells) * 3.3F;
            bat->load_reconnect_voltage  = static_cast<float>(num_cells) * 3.6F;

            // 5% voltage drop at max current
            bat->internal_resistance     = bat->load_disconnect_voltage * 0.05F /
                DISCHARGE_CURRENT_MAX;

            bat->absolute_min_voltage    = static_cast<float>(num_cells) * 2.5F;

            bat->ocv_full                = static_cast<float>(num_cells) * 4.0F;
            bat->ocv_empty               = static_cast<float>(num_cells) * 3.0F;

            // C/10 cut-off at end of CV phase by default
            bat->topping_cutoff_current  = bat->nominal_capacity / 10;

            bat->float_enabled = false;
            bat->equalization_enabled = false;
            bat->temperature_compensation = 0;
            bat->charge_temp_min = 0;
            break;

        case BAT_TYPE_CUSTOM:
#ifdef CONFIG_BAT_TYPE_CUSTOM
            bat->absolute_max_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_ABS_MAX_VOLTAGE_MV);

            bat->topping_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_TOPPING_VOLTAGE_MV);

            bat->recharge_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_RECHARGE_VOLTAGE_MV);

            bat->load_disconnect_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_DISCONNECT_VOLTAGE_MV);
            bat->load_reconnect_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_RECONNECT_VOLTAGE_MV);

            bat->internal_resistance = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_INTERNAL_RESISTANCE_MOHM);

            bat->absolute_min_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_ABS_MIN_VOLTAGE_MV);

            // Voltages during idle (no charging/discharging current)
            bat->ocv_full = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_OCV_FULL_MV);
            bat->ocv_empty = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_OCV_EMPTY_MV);

            // https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
            bat->topping_cutoff_current = bat->nominal_capacity * 0.04F;  // 3-5 % of C/1

            bat->float_enabled = IS_ENABLED(CONFIG_CELL_FLOAT);
            bat->float_recharge_time = CONFIG_CELL_FLOAT_RECHARGE_TIME;
            bat->float_voltage = 0.001F *
                static_cast<float>(CONFIG_BAT_NUM_CELLS * CONFIG_CELL_FLOAT_VOLTAGE_MV);

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
#else
            LOG_ERR("Custom battery type cannot be initialized at runtime.");
#endif
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
    //

    struct condition {
        std::function<bool()> func;
        const char *text;
    };

    const condition conditions[] = {
        {.func =
             [bat_conf]() {
                 return bat_conf->load_reconnect_voltage >
                        (bat_conf->load_disconnect_voltage + 0.4);
             },
         .text = "Load Reconnect Voltage must be higher than Load Disconnect Voltage + 0.4"},
        {.func =
             [bat_conf]() {
                 return bat_conf->recharge_voltage < (bat_conf->topping_voltage - 0.4);
             },
         .text = "Recharge Voltage must be lower than Topping Voltage - 0.4"},
        {.func =
             [bat_conf]() {
                 return bat_conf->recharge_voltage > (bat_conf->load_disconnect_voltage + 1);
             },
         .text = "Recharge Voltage must be higher than Load Disconnect Voltage + 1.0"},
        {.func =
             [bat_conf]() {
                 return bat_conf->load_disconnect_voltage > (bat_conf->absolute_min_voltage + 0.4);
             },
         .text = "Load Disconnecct Voltage must be higher than Absolute Min Voltage + 0.4"},
        {.func =
             [bat_conf]() {
                 return bat_conf->internal_resistance <
                        bat_conf->load_disconnect_voltage * 0.1 / DISCHARGE_CURRENT_MAX;
             },
         .text = "Internal Battery Resistance must not cause more than 10% drop at Max Discharge "
                 "Current"},
        {.func =
             [bat_conf]() {
                 return bat_conf->wire_resistance <
                        bat_conf->topping_voltage * 0.03 / DISCHARGE_CURRENT_MAX;
             },
         .text = "Wire Resistances must not cause more than 3% drop at Max Discharge Current"},
        {.func =
             [bat_conf]() {
                 return bat_conf->topping_cutoff_current < (bat_conf->nominal_capacity / 10.0);
             },
         .text = "Topping Cutoff Current must be less than 10% of Nominal Capacity (C/10)"},
        {.func = [bat_conf]() { return bat_conf->topping_cutoff_current > 0.01; },
         .text = "Topping Cutoff Current must be higher than 0.01A"},
        {.func =
             [bat_conf]() {
                 return bat_conf->float_enabled == false ||
                        bat_conf->float_voltage < bat_conf->topping_voltage;
             },
         .text = "Floating Charge Voltage must be lower than Topping Voltage"},
        {.func =
             [bat_conf]() {
                 return bat_conf->float_enabled == false ||
                        bat_conf->float_voltage > bat_conf->load_disconnect_voltage;
             },
         .text = "Floating Charge Voltage must be higher than Topping Voltage"},
    };

    bool result = true;
    for (auto cond : conditions) {
        bool cond_res = cond.func();
        if (cond_res == false) {
            LOG_ERR("battery_conf_check: failed condition '%s'", cond.text);
        }
        result = result && cond_res;
    }

    return result;
}

void battery_conf_overwrite(BatConf *source, BatConf *destination, Charger *charger)
{
    // TODO: stop DC/DC

    destination->topping_voltage                = source->topping_voltage;
    destination->recharge_voltage               = source->recharge_voltage;
    destination->load_reconnect_voltage         = source->load_reconnect_voltage;
    destination->load_disconnect_voltage        = source->load_disconnect_voltage;
    destination->absolute_max_voltage           = source->absolute_max_voltage;
    destination->absolute_min_voltage           = source->absolute_min_voltage;
    destination->charge_current_max             = source->charge_current_max;
    destination->topping_cutoff_current         = source->topping_cutoff_current;
    destination->topping_duration               = source->topping_duration;
    destination->float_enabled                  = source->float_enabled;
    destination->float_voltage                  = source->float_voltage;
    destination->float_recharge_time            = source->float_recharge_time;
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
        a->recharge_voltage               != b->recharge_voltage ||
        a->load_reconnect_voltage         != b->load_reconnect_voltage ||
        a->load_disconnect_voltage        != b->load_disconnect_voltage ||
        a->absolute_max_voltage           != b->absolute_max_voltage ||
        a->absolute_min_voltage           != b->absolute_min_voltage ||
        a->charge_current_max             != b->charge_current_max ||
        a->topping_cutoff_current         != b->topping_cutoff_current ||
        a->topping_duration               != b->topping_duration ||
        a->float_enabled                  != b->float_enabled ||
        a->float_voltage                  != b->float_voltage ||
        a->float_recharge_time            != b->float_recharge_time ||
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
    if (port->bus->voltage > bat->absolute_min_voltage * 2 &&
        port->bus->voltage < bat->absolute_max_voltage * 2)
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
#if BOARD_HAS_LOAD_OUTPUT || \
    BOARD_HAS_USB_OUTPUT

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
        if (port->bus->voltage < port->bus->src_control_voltage(bat_conf->absolute_min_voltage)) {
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
            port->bus->src_control_voltage(bat_conf->absolute_min_voltage + 0.1F))
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
        port->bus->voltage < (bat_conf->absolute_max_voltage - 0.5F) *
                             port->bus->series_multiplier)
    {
        dev_stat.clear_error(ERR_BAT_OVERVOLTAGE);
    }

    if (state != CHG_STATE_FOLLOWER && (uptime() - time_last_ctrl_msg) <= 1) {
        enter_state(CHG_STATE_FOLLOWER);
    }

    // state machine
    switch (state) {
        case CHG_STATE_IDLE: {
            if ((time_state_changed == CHARGER_TIME_NEVER ||
                    ((uptime() - time_state_changed) > bat_conf->time_limit_recharge &&
                    port->bus->voltage < port->bus->sink_control_voltage(
                        bat_conf->recharge_voltage))
                )
                && port->bus->voltage > port->bus->sink_control_voltage(
                    bat_conf->absolute_min_voltage) // ToDo set error flag otherwise
                && bat_temperature < bat_conf->charge_temp_max - 1
                && bat_temperature > bat_conf->charge_temp_min + 1)
            {
                port->bus->sink_voltage_intercept = bat_conf->topping_voltage +
                    bat_conf->temperature_compensation * (bat_temperature - 25);
                port->pos_current_limit = bat_conf->charge_current_max;
                target_current_control = port->pos_current_limit;
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

            // power sharing: multiple devices in parallel supply the same current
            target_current_control = port->current_filtered;

            if (port->bus->voltage_filtered >= port->bus->sink_control_voltage() - 0.05F) {
                // battery is full if topping target voltage is still reached (i.e. sufficient
                // solar power available) and time limit or cut-off current reached
                if (port->current_filtered < bat_conf->topping_cutoff_current ||
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
                else if (bat_conf->float_enabled) {
                    port->bus->sink_voltage_intercept = bat_conf->float_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25);
                    enter_state(CHG_STATE_FLOAT);
                }
                else {
                    port->pos_current_limit = 0;
                    enter_state(CHG_STATE_IDLE);
                }
            }
            break;
        }
        case CHG_STATE_FLOAT: {
            // continuously adjust voltage setting for temperature compensation
            port->bus->sink_voltage_intercept = bat_conf->float_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            target_current_control = port->current_filtered;

            if (port->bus->voltage >= port->bus->sink_control_voltage()) {
                time_target_voltage_reached = uptime();
            }

            if ((uptime() - time_target_voltage_reached > bat_conf->float_recharge_time) &&
                port->bus->voltage_filtered < port->bus->sink_control_voltage(
                    bat_conf->recharge_voltage))
            {
                // the battery was discharged: float voltage could not be reached anymore
                port->pos_current_limit = bat_conf->charge_current_max;
                full = false;
                // assumption: float does not harm the battery --> never go back to idle
                // (for Li-ion battery: disable float!)
                enter_state(CHG_STATE_BULK);
            }
            break;
        }
        case CHG_STATE_EQUALIZATION: {
            // continuously adjust voltage setting for temperature compensation
            port->bus->sink_voltage_intercept = bat_conf->equalization_voltage +
                bat_conf->temperature_compensation * (bat_temperature - 25);

            target_current_control = port->current_filtered;

            // current or time limit for equalization reached
            if (uptime() - time_state_changed > bat_conf->equalization_duration)
            {
                // reset triggers
                time_last_equalization = uptime();
                deep_dis_last_equalization = num_deep_discharges;

                discharged_Ah = 0;         // reset coulomb counter again

                if (bat_conf->float_enabled) {
                    port->bus->sink_voltage_intercept = bat_conf->float_voltage +
                        bat_conf->temperature_compensation * (bat_temperature - 25);
                    enter_state(CHG_STATE_FLOAT);
                }
                else {
                    port->pos_current_limit = 0;
                    enter_state(CHG_STATE_IDLE);
                }
            }
            break;
        }
        case CHG_STATE_FOLLOWER: {
            if ((uptime() - time_last_ctrl_msg) > 1) {
                // go back to normal state machine
                port->pos_current_limit = bat_conf->charge_current_max;
                enter_state(CHG_STATE_BULK);
            }
            else {
                // set current target as received from external device
                port->pos_current_limit = target_current_control;
                // set safety limit for voltage
                port->bus->sink_voltage_intercept = bat_conf->absolute_max_voltage;
            }
            break;
        }
    }
}

void Charger::init_terminal(BatConf *bat) const
{
    port->bus->sink_voltage_intercept = bat->topping_voltage;
    port->bus->src_voltage_intercept = bat->load_disconnect_voltage;

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
