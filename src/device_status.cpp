/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_status.h"

#include <zephyr.h>

#include <math.h>       // for fabs function
#include <stdio.h>

#include "setup.h"
#include "helper.h"

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void DeviceStatus::update_energy()
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
        solar_in_total_Wh_prev = solar_in_total_Wh;
        load_out_total_Wh_prev = load_out_total_Wh;
        bat_chg_total_Wh_prev = bat_chg_total_Wh;
        bat_dis_total_Wh_prev = bat_dis_total_Wh;
        #if CONFIG_HV_TERMINAL_NANOGRID
        grid_import_total_Wh_prev = grid_import_total_Wh;
        grid_export_total_Wh_prev = grid_export_total_Wh;
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
        if (seconds_zero_solar > 60*60*5) {
            day_counter++;
            solar_in_total_Wh_prev = solar_in_total_Wh;
            load_out_total_Wh_prev = load_out_total_Wh;
            bat_chg_total_Wh_prev = bat_chg_total_Wh;
            bat_dis_total_Wh_prev = bat_dis_total_Wh;
            #if CONFIG_HV_TERMINAL_NANOGRID
            grid_import_total_Wh_prev = grid_import_total_Wh;
            grid_export_total_Wh_prev = grid_export_total_Wh;
            #endif
            solar_terminal.neg_energy_Wh = 0.0;
            #if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
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

    bat_chg_total_Wh = bat_chg_total_Wh_prev +
        (bat_terminal.pos_energy_Wh > 0 ? bat_terminal.pos_energy_Wh : 0);
    bat_dis_total_Wh = bat_dis_total_Wh_prev +
        (bat_terminal.neg_energy_Wh > 0 ? bat_terminal.neg_energy_Wh : 0);

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    solar_in_total_Wh = solar_in_total_Wh_prev +
        (solar_terminal.neg_energy_Wh > 0 ? solar_terminal.neg_energy_Wh : 0);
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    load_out_total_Wh = load_out_total_Wh_prev +
        (load.pos_energy_Wh > 0 ? load.pos_energy_Wh : 0);
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    grid_import_total_Wh = grid_import_total_Wh_prev +
        (grid_terminal.neg_energy_Wh > 0 ? grid_terminal.neg_energy_Wh : 0);
    grid_export_total_Wh = grid_export_total_Wh_prev +
        (grid_terminal.pos_energy_Wh > 0 ? grid_terminal.pos_energy_Wh : 0);
#endif
}

void DeviceStatus::update_min_max_values()
{
    if (bat_terminal.bus->voltage > battery_voltage_max) {
        battery_voltage_max = bat_terminal.bus->voltage;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR
    if (solar_terminal.bus->voltage > solar_voltage_max) {
        solar_voltage_max = solar_terminal.bus->voltage;
    }
#elif CONFIG_PWM_TERMINAL_SOLAR
    if (pwm_switch.ext_voltage > solar_voltage_max) {
        solar_voltage_max = pwm_switch.ext_voltage;
    }
#endif

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    if (dcdc.inductor_current > dcdc_current_max) {
        dcdc_current_max = dcdc.inductor_current;
    }

    if (dcdc.temp_mosfets > mosfet_temp_max) {
        mosfet_temp_max = dcdc.temp_mosfets;
    }
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    if (load.current > load_current_max) {
        load_current_max = load.current;
    }
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    if (-solar_terminal.power > solar_power_max_day) {
        solar_power_max_day = -solar_terminal.power;
        if (solar_power_max_day > solar_power_max_total) {
            solar_power_max_total = solar_power_max_day;
        }
    }
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    if (load.power > load_power_max_day) {
        load_power_max_day = load.power;
        if (load_power_max_day > load_power_max_total) {
            load_power_max_total = load_power_max_day;
        }
    }
#endif

    if (charger.bat_temperature > static_cast<float>(bat_temp_max)) {
        bat_temp_max = charger.bat_temperature;
    }

    if (internal_temp > static_cast<float>(int_temp_max)) {
        int_temp_max = internal_temp;
    }
}
