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

#include "log.h"
#include "config.h"
#include "pcb.h"

#include "dc_bus.h"
#include "load.h"

#include <math.h>       // for fabs function
#include <stdio.h>

extern float mcu_temp;

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void log_update_energy(LogData *log_data, DcBus *solar, DcBus *bat, DcBus *load)
{
    // static variables so that it is not reset for each function call
    static int seconds_zero_solar = 0;

    // stores the input/output energy status of previous day and to add
    // xxx_day_Wh only once per day and increase accuracy
    static uint32_t solar_in_total_Wh_prev = log_data->solar_in_total_Wh;
    static uint32_t load_out_total_Wh_prev = log_data->load_out_total_Wh;
    static uint32_t bat_chg_total_Wh_prev = log_data->bat_chg_total_Wh;
    static uint32_t bat_dis_total_Wh_prev = log_data->bat_dis_total_Wh;

    if (solar->voltage < bat->voltage) {
        seconds_zero_solar += 1;
    }
    else {
        // solar voltage > battery voltage after 5 hours of night time means sunrise in the morning
        // --> reset daily energy counters
        if (seconds_zero_solar > 60*60*5) {
            //printf("Night!\n");
            log_data->day_counter++;
            solar_in_total_Wh_prev = log_data->solar_in_total_Wh;
            load_out_total_Wh_prev = log_data->load_out_total_Wh;
            bat_chg_total_Wh_prev = log_data->bat_chg_total_Wh;
            bat_dis_total_Wh_prev = log_data->bat_dis_total_Wh;
            solar->dis_energy_Wh = 0.0;
            load->chg_energy_Wh = 0.0;
            bat->chg_energy_Wh = 0.0;
            bat->dis_energy_Wh = 0.0;
        }
        seconds_zero_solar = 0;
    }

    log_data->bat_chg_total_Wh = bat_chg_total_Wh_prev + (bat->chg_energy_Wh > 0 ? bat->chg_energy_Wh : 0);
    log_data->bat_dis_total_Wh = bat_dis_total_Wh_prev + (bat->dis_energy_Wh > 0 ? bat->dis_energy_Wh : 0);
    log_data->solar_in_total_Wh = solar_in_total_Wh_prev + (solar->dis_energy_Wh > 0 ? solar->dis_energy_Wh : 0);
    log_data->load_out_total_Wh = load_out_total_Wh_prev + (load->chg_energy_Wh > 0 ? load->chg_energy_Wh : 0);
}

void log_update_min_max_values(LogData *log_data, Dcdc *dcdc, Charger *charger, LoadOutput *load, DcBus *solar_bus, DcBus *bat_bus, DcBus *load_bus)
{
    if (bat_bus->voltage > log_data->battery_voltage_max) {
        log_data->battery_voltage_max = bat_bus->voltage;
    }

    if (solar_bus->voltage > log_data->solar_voltage_max) {
        log_data->solar_voltage_max = solar_bus->voltage;
    }

    if (dcdc->ls_current > log_data->dcdc_current_max) {
        log_data->dcdc_current_max = dcdc->ls_current;
    }

    if (load->bus->current > log_data->load_current_max) {
        log_data->load_current_max = load->bus->current;
    }

    if (solar_bus->current < 0) {
        uint16_t solar_power = -solar_bus->power;
        if (solar_power > log_data->solar_power_max_day) {
            log_data->solar_power_max_day = solar_power;
            if (log_data->solar_power_max_day > log_data->solar_power_max_total) {
                log_data->solar_power_max_total = log_data->solar_power_max_day;
            }
        }
    }

    if (load->bus->current > 0) {
        uint16_t load_power = load->bus->voltage * load->bus->current;
        if (load_power > log_data->load_power_max_day) {
            log_data->load_power_max_day = load_power;
            if (log_data->load_power_max_day > log_data->load_power_max_total) {
                log_data->load_power_max_total = log_data->load_power_max_day;
            }
        }
    }

    if (dcdc->temp_mosfets > log_data->mosfet_temp_max) {
        log_data->mosfet_temp_max = dcdc->temp_mosfets;
    }

    if (charger->bat_temperature > log_data->bat_temp_max) {
        log_data->bat_temp_max = charger->bat_temperature;
    }

    if (mcu_temp > log_data->int_temp_max) {
        log_data->int_temp_max = mcu_temp;
    }
}