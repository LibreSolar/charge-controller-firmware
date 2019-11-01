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

#include "device_status.h"
#include "config.h"
#include "pcb.h"

#include "main.h"

#include <math.h>       // for fabs function
#include <stdio.h>

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void DeviceStatus::update_energy()
{
    // static variables so that it is not reset for each function call
    static int seconds_zero_solar = 0;

    // stores the input/output energy status of previous day and to add
    // xxx_day_Wh only once per day and increase accuracy
    static uint32_t solar_in_total_Wh_prev = solar_in_total_Wh;
    static uint32_t load_out_total_Wh_prev = load_out_total_Wh;
    static uint32_t bat_chg_total_Wh_prev = bat_chg_total_Wh;
    static uint32_t bat_dis_total_Wh_prev = bat_dis_total_Wh;

    if (solar_terminal.voltage < bat_terminal.voltage) {
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
            load_terminal.pos_energy_Wh = 0.0;
            bat_terminal.pos_energy_Wh = 0.0;
            bat_terminal.neg_energy_Wh = 0.0;
        }
        seconds_zero_solar = 0;
    }

    bat_chg_total_Wh = bat_chg_total_Wh_prev +
        (bat_terminal.pos_energy_Wh > 0 ? bat_terminal.pos_energy_Wh : 0);
    bat_dis_total_Wh = bat_dis_total_Wh_prev +
        (bat_terminal.neg_energy_Wh > 0 ? bat_terminal.neg_energy_Wh : 0);
    solar_in_total_Wh = solar_in_total_Wh_prev +
        (solar_terminal.neg_energy_Wh > 0 ? solar_terminal.neg_energy_Wh : 0);
    load_out_total_Wh = load_out_total_Wh_prev +
        (load_terminal.pos_energy_Wh > 0 ? load_terminal.pos_energy_Wh : 0);
}

void DeviceStatus::update_min_max_values()
{
    if (bat_terminal.voltage > battery_voltage_max) {
        battery_voltage_max = bat_terminal.voltage;
    }

    if (solar_terminal.voltage > solar_voltage_max) {
        solar_voltage_max = solar_terminal.voltage;
    }

#if FEATURE_DCDC_CONVERTER
    if (dcdc.lvs->current > dcdc_current_max) {
        dcdc_current_max = dcdc.lvs->current;
    }

    if (dcdc.temp_mosfets > mosfet_temp_max) {
        mosfet_temp_max = dcdc.temp_mosfets;
    }
#endif

    if (load_terminal.current > load_current_max) {
        load_current_max = load_terminal.current;
    }

    if (-solar_terminal.power > solar_power_max_day) {
        solar_power_max_day = -solar_terminal.power;
        if (solar_power_max_day > solar_power_max_total) {
            solar_power_max_total = solar_power_max_day;
        }
    }

    if (load_terminal.power > load_power_max_day) {
        load_power_max_day = load_terminal.power;
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
