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

#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

/** @file
 *
 * @brief
 * Device-level data storage and functions (like max/min values, error flags, etc.)
 */

#include "bat_charger.h"
#include "power_port.h"
#include "load.h"
#include "dcdc.h"
#include <stdbool.h>
#include <stdint.h>

/** Error Flags
 */
enum ErrorFlag {
    ERR_HS_MOSFET_SHORT = 0,        ///< Short-circuit in HS MOSFET
    ERR_BAT_OVERVOLTAGE,
    ERR_BAT_UNDERVOLTAGE,
    ERR_BAT_DIS_OVERTEMP,
    ERR_BAT_DIS_UNDERTEMP,
    ERR_BAT_CHG_OVERTEMP,
    ERR_BAT_CHG_UNDERTEMP,
    ERR_LOAD_OVERVOLTAGE,           ///< To high voltage for load so that it was switched off
    ERR_LOAD_SHORT_CIRCUIT,         ///< Short circuit detected by load port
    ERR_LOAD_OVERCURRENT,           ///< Long-term overcurrent of charge controller
    ERR_LOAD_VOLTAGE_DIP,           ///< Overcurrent identified via voltage dip (may be because of
                                    ///< too small battery)
    ERR_INT_OVERTEMP                ///< Charge controller internal temperature too high
};

/** Device Status data
 *
 * Stores error counters and some maximum ever measured values to EEPROM
 */
class DeviceStatus
{
public:

    /** Updates the total energy counters for solar, battery and load bus
     */
    void update_energy();

    /** Updates the logged min/max values for voltages, power, temperatures etc.
     */
    void update_min_max_values();

    // total energy
    uint32_t bat_chg_total_Wh;
    uint32_t bat_dis_total_Wh;
    uint32_t solar_in_total_Wh;
    uint32_t load_out_total_Wh;

    // maximum/minimum values
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

    // instantaneous device-level data
    uint32_t error_flags;       ///< Currently detected errors
    float internal_temp;        ///< Internal temperature (measured in MCU)
};

#endif /* DEVICE_STATUS_H */
