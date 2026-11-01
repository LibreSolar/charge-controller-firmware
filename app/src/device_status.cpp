/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_status.h"

#include "bat_charger.h"
#include "dcdc.h"
#include "load.h"
#include "power_port.h"

#include <zephyr/kernel.h>

#include <math.h> // for fabs function
#include <stdio.h>

#include "helper.h"
#include "setup.h"

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void device_status_update_energy(struct device_status *ds)
{
    // static variables so that it is not reset for each function call
    static int seconds_zero_solar = 0;

    // stores the input/output energy status of previous day and to add
    // xxx_day_Wh only once per day and increase accuracy
    static uint32_t solar_in_total_Wh_prev;
    static uint32_t load_out_total_Wh_prev;
    static uint32_t bat_chg_total_Wh_prev;
    static uint32_t bat_dis_total_Wh_prev;
#if CONFIG_HV_TERMINAL_NANOGRID
    static uint32_t grid_import_total_Wh_prev;
    static uint32_t grid_export_total_Wh_prev;
#endif

    static bool first_call = true;
    if (first_call) {
        // initialize values with values we got from EEPROM
        solar_in_total_Wh_prev = ds->solar_in_total_Wh;
        load_out_total_Wh_prev = ds->load_out_total_Wh;
        bat_chg_total_Wh_prev = ds->bat_chg_total_Wh;
        bat_dis_total_Wh_prev = ds->bat_dis_total_Wh;
#if CONFIG_HV_TERMINAL_NANOGRID
        grid_import_total_Wh_prev = ds->grid_import_total_Wh;
        grid_export_total_Wh_prev = ds->grid_export_total_Wh;
#endif
        first_call = false;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR
    if (solar_terminal.bus->voltage < bat_terminal.bus->voltage) {
#else
    if (pwm_switch.ext_voltage < bat_terminal.bus->voltage) {
#endif
        seconds_zero_solar += 1;
    }
    else {
        // solar voltage > battery voltage after 5 hours of night time means sunrise in the morning
        // --> reset daily energy counters
        if (seconds_zero_solar > 60 * 60 * 5) {
            ds->day_counter++;
            solar_in_total_Wh_prev = ds->solar_in_total_Wh;
            load_out_total_Wh_prev = ds->load_out_total_Wh;
            bat_chg_total_Wh_prev = ds->bat_chg_total_Wh;
            bat_dis_total_Wh_prev = ds->bat_dis_total_Wh;
#if CONFIG_HV_TERMINAL_NANOGRID
            grid_import_total_Wh_prev = ds->grid_import_total_Wh;
            grid_export_total_Wh_prev = ds->grid_export_total_Wh;
#endif
            solar_terminal.neg_energy_Wh = 0.0;
#if BOARD_HAS_LOAD_OUTPUT
            load.pos_energy_Wh = 0.0;
#endif
            bat_terminal.pos_energy_Wh = 0.0;
            bat_terminal.neg_energy_Wh = 0.0;
#if CONFIG_HV_TERMINAL_NANOGRID
            grid_terminal.pos_energy_Wh = 0.0;
            grid_terminal.neg_energy_Wh = 0.0;
#endif
        }
        seconds_zero_solar = 0;
    }
#endif

    ds->bat_chg_total_Wh =
        bat_chg_total_Wh_prev + (bat_terminal.pos_energy_Wh > 0 ? bat_terminal.pos_energy_Wh : 0);
    ds->bat_dis_total_Wh =
        bat_dis_total_Wh_prev + (bat_terminal.neg_energy_Wh > 0 ? bat_terminal.neg_energy_Wh : 0);

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    ds->solar_in_total_Wh = solar_in_total_Wh_prev
                            + (solar_terminal.neg_energy_Wh > 0 ? solar_terminal.neg_energy_Wh : 0);
#endif

#if BOARD_HAS_LOAD_OUTPUT
    ds->load_out_total_Wh =
        load_out_total_Wh_prev + (load.pos_energy_Wh > 0 ? load.pos_energy_Wh : 0);
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    ds->grid_import_total_Wh =
        grid_import_total_Wh_prev
        + (grid_terminal.neg_energy_Wh > 0 ? grid_terminal.neg_energy_Wh : 0);
    ds->grid_export_total_Wh =
        grid_export_total_Wh_prev
        + (grid_terminal.pos_energy_Wh > 0 ? grid_terminal.pos_energy_Wh : 0);
#endif
}

void device_status_update_min_max_values(struct device_status *ds)
{
    if (bat_terminal.bus->voltage > ds->battery_voltage_max) {
        ds->battery_voltage_max = bat_terminal.bus->voltage;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR
    if (solar_terminal.bus->voltage > ds->solar_voltage_max) {
        ds->solar_voltage_max = solar_terminal.bus->voltage;
    }
#elif CONFIG_PWM_TERMINAL_SOLAR
    if (pwm_switch.ext_voltage > ds->solar_voltage_max) {
        ds->solar_voltage_max = pwm_switch.ext_voltage;
    }
#endif

#if BOARD_HAS_DCDC
    if (dcdc.inductor_current > ds->dcdc_current_max) {
        ds->dcdc_current_max = dcdc.inductor_current;
    }

    if (dcdc.temp_mosfets > ds->mosfet_temp_max) {
        ds->mosfet_temp_max = dcdc.temp_mosfets;
    }
#endif

#if BOARD_HAS_LOAD_OUTPUT
    if (load.current > ds->load_current_max) {
        ds->load_current_max = load.current;
    }
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    if (-solar_terminal.power > ds->solar_power_max_day) {
        ds->solar_power_max_day = -solar_terminal.power;
        if (ds->solar_power_max_day > ds->solar_power_max_total) {
            ds->solar_power_max_total = ds->solar_power_max_day;
        }
    }
#endif

#if BOARD_HAS_LOAD_OUTPUT
    if (load.power > ds->load_power_max_day) {
        ds->load_power_max_day = load.power;
        if (ds->load_power_max_day > ds->load_power_max_total) {
            ds->load_power_max_total = ds->load_power_max_day;
        }
    }
#endif

    if (charger.bat_temperature > static_cast<float>(ds->bat_temp_max)) {
        ds->bat_temp_max = charger.bat_temperature;
    }

    if (ds->internal_temp > static_cast<float>(ds->int_temp_max)) {
        ds->int_temp_max = ds->internal_temp;
    }
}
