/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "device_status.h"

#include <math.h>       // for fabs function
#include <stdio.h>

#include "config.h"
#include "pcb.h"
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

    static bool first_call = true;
    if (first_call) {
        // initialize values with values we got from EEPROM
        solar_in_total_Wh_prev = solar_in_total_Wh;
        load_out_total_Wh_prev = load_out_total_Wh;
        bat_chg_total_Wh_prev = bat_chg_total_Wh;
        bat_dis_total_Wh_prev = bat_dis_total_Wh;
        first_call = false;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    if (solar_terminal.bus->voltage < bat_terminal.bus->voltage) {
        seconds_zero_solar += 1;
    }
    else {
        // solar voltage > battery voltage after 5 hours of night time means sunrise in the morning
        // --> reset daily energy counters
        if (seconds_zero_solar > 60*60*5) {
            //printf("Night!\n");
            day_counter++;
            solar_in_total_Wh_prev = solar_in_total_Wh;
            load_out_total_Wh_prev = load_out_total_Wh;
            bat_chg_total_Wh_prev = bat_chg_total_Wh;
            bat_dis_total_Wh_prev = bat_dis_total_Wh;
            solar_terminal.neg_energy_Wh = 0.0;
            load.pos_energy_Wh = 0.0;
            bat_terminal.pos_energy_Wh = 0.0;
            bat_terminal.neg_energy_Wh = 0.0;
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
    load_out_total_Wh = load_out_total_Wh_prev +
        (load.pos_energy_Wh > 0 ? load.pos_energy_Wh : 0);
}

void DeviceStatus::update_min_max_values()
{
    if (bat_terminal.bus->voltage > battery_voltage_max) {
        battery_voltage_max = bat_terminal.bus->voltage;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    if (solar_terminal.bus->voltage > solar_voltage_max) {
        solar_voltage_max = solar_terminal.bus->voltage;
    }
#endif

#if CONFIG_HAS_DCDC_CONVERTER
    if (dcdc.lvs->current > dcdc_current_max) {
        dcdc_current_max = dcdc.lvs->current;
    }

    if (dcdc.temp_mosfets > mosfet_temp_max) {
        mosfet_temp_max = dcdc.temp_mosfets;
    }
#endif

    if (load.current > load_current_max) {
        load_current_max = load.current;
    }

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    if (-solar_terminal.power > solar_power_max_day) {
        solar_power_max_day = -solar_terminal.power;
        if (solar_power_max_day > solar_power_max_total) {
            solar_power_max_total = solar_power_max_day;
        }
    }
#endif

    if (load.power > load_power_max_day) {
        load_power_max_day = load.power;
        if (load_power_max_day > load_power_max_total) {
            load_power_max_total = load_power_max_day;
        }
    }

    if (charger.bat_temperature > bat_temp_max) {
        bat_temp_max = charger.bat_temperature;
    }

    if (internal_temp > int_temp_max) {
        int_temp_max = internal_temp;
    }
}
