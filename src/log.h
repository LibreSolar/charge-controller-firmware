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

#ifndef LOG_H
#define LOG_H

/** @file
 *
 * @brief
 * Structs needed for data logging (like max/min values, etc.)
 */

#include <stdbool.h>
#include <stdint.h>

/** Log Data
 *
 * Stores error counters and some maximum ever measured values to EEPROM
 */
typedef struct {
    float solar_in_day_Wh;
    float load_out_day_Wh;
    uint32_t solar_in_total_Wh;
    uint32_t load_out_total_Wh;

    uint16_t solar_power_max_day;
    uint16_t load_power_max_day;
    uint16_t solar_power_max_total;
    uint16_t load_power_max_total;

    float battery_voltage_max;
    float solar_voltage_max;
    float dcdc_current_max;
    float load_current_max;
    int bat_temp_max;         // °C
    int int_temp_max;         // °C
    int mosfet_temp_max;
    int day_counter;
} log_data_t;

#endif /* LOG_H */
