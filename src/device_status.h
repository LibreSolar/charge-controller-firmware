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
    ERR_BAT_UNDERVOLTAGE = 0,       ///< Battery voltage too low
    ERR_BAT_OVERVOLTAGE = 1,        ///< Battery voltage too high
    ERR_BAT_DIS_OVERCURRENT = 2,    ///< Battery discharge overcurrent
    ERR_BAT_CHG_OVERCURRENT = 3,    ///< Battery charge overcurrent
    ERR_BAT_DIS_UNDERTEMP = 4,      ///< Temperature below discharge minimum limit
    ERR_BAT_DIS_OVERTEMP = 5,       ///< Temperature above discharge maximum limit
    ERR_BAT_CHG_UNDERTEMP = 6,      ///< Temperature below charge minimum limit
    ERR_BAT_CHG_OVERTEMP = 7,       ///< Temperature above charge maximum limit
    ERR_LOAD_OVERVOLTAGE = 8,       ///< To high voltage for load so that it was switched off
    ERR_LOAD_OVERCURRENT = 9,       ///< Long-term overcurrent of load port
    ERR_LOAD_SHORT_CIRCUIT = 10,    ///< Short circuit detected at load port
    ERR_LOAD_VOLTAGE_DIP = 11,      ///< Overcurrent identified via voltage dip (may be because of
                                    ///< too small battery)
    ERR_INT_OVERTEMP = 12,          ///< Charge controller internal temperature too high
    ERR_DCDC_HS_MOSFET_SHORT = 13,  ///< Short-circuit in HS MOSFET
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

    void set_error(ErrorFlag e) { error_flags |= (1U << e); }
    void clear_error(ErrorFlag e) { error_flags &= ~(1U << e); }
    bool has_error(ErrorFlag e) { return (error_flags & (1U << e)) != 0; }
};

#endif /* DEVICE_STATUS_H */
