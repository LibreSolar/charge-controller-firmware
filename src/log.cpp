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

//----------------------------------------------------------------------------
// must be called exactly once per second, otherwise energy calculation gets wrong
void log_update_energy(log_data_t *log_data, dc_bus_t *bat, dc_bus_t *solar, dc_bus_t *load)
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
    log_data->load_out_total_Wh = load_out_total_Wh_prev + (load->chg_energy_Wh > 0 ? load->dis_energy_Wh : 0);
}
