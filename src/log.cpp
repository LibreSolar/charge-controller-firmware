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

#include "main.h"

#include <math.h>       // for fabs function
#include <stdio.h>

extern float mcu_temp;

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void log_update_energy(LogData *log_data, PowerPort *solar, PowerPort *bat, PowerPort *load)
{
    // static variables so that it is not reset for each function call
    static int seconds_zero_solar = 0;

    // stores the input/output energy status of previous day and to add
    // xxx_day_Wh only once per day and increase accuracy
    static uint32_t solar_in_total_Wh_prev = log_data->solar_in_total_Wh;
    static uint32_t load_out_total_Wh_prev = log_data->load_out_total_Wh;
    static uint32_t bat_chg_total_Wh_prev = log_data->bat_chg_total_Wh;
    static uint32_t bat_dis_total_Wh_prev = log_data->bat_dis_total_Wh;

    if (solar->bus->voltage < bat->bus->voltage) {
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

    log_data->bat_chg_total_Wh = bat_chg_total_Wh_prev +
        (bat->chg_energy_Wh > 0 ? bat->chg_energy_Wh : 0);
    log_data->bat_dis_total_Wh = bat_dis_total_Wh_prev +
        (bat->dis_energy_Wh > 0 ? bat->dis_energy_Wh : 0);
    log_data->solar_in_total_Wh = solar_in_total_Wh_prev +
        (solar->dis_energy_Wh > 0 ? solar->dis_energy_Wh : 0);
    log_data->load_out_total_Wh = load_out_total_Wh_prev +
        (load->chg_energy_Wh > 0 ? load->chg_energy_Wh : 0);
}

void log_update_min_max_values(LogData *log_data, Dcdc *dcdc, Charger *charger, LoadOutput *load,
    PowerPort *solar_port, PowerPort *bat_port, PowerPort *load_port)
{
    if (bat_port->bus->voltage > log_data->battery_voltage_max) {
        log_data->battery_voltage_max = bat_port->bus->voltage;
    }

    if (solar_port->bus->voltage > log_data->solar_voltage_max) {
        log_data->solar_voltage_max = solar_port->bus->voltage;
    }

    if (dcdc->lvs->current > log_data->dcdc_current_max) {
        log_data->dcdc_current_max = dcdc->lvs->current;
    }

    if (load->port->current > log_data->load_current_max) {
        log_data->load_current_max = load->port->current;
    }

    if (-solar_port->power > log_data->solar_power_max_day) {
        log_data->solar_power_max_day = -solar_port->power;
        if (log_data->solar_power_max_day > log_data->solar_power_max_total) {
            log_data->solar_power_max_total = log_data->solar_power_max_day;
        }
    }

    if (load->port->power > log_data->load_power_max_day) {
        log_data->load_power_max_day = load->port->power;
        if (log_data->load_power_max_day > log_data->load_power_max_total) {
            log_data->load_power_max_total = log_data->load_power_max_day;
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