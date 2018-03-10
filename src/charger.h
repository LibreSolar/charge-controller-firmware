/* mbed library for a battery charge controller
 * Copyright (c) 2017 Martin Jäger (www.libre.solar)
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

// settings: battery_capacity, charge_profile
// input:  actual charge current, actual battery voltage
// output: SOC, target_voltage, target_current (=max current)

#ifndef CHARGECONTROLLER_H
#define CHARGECONTROLLER_H

#include "mbed.h"

// default values for 12V lead-acid battery
struct ChargingProfile {
    int num_cells = 6;

    // State: Standby
    int time_limit_recharge = 60; // sec
    float cell_voltage_recharge = 2.3; // V
    float cell_voltage_absolute_min = 1.8; // V   (under this voltage, battery is considered damaged)

    // State: CC/bulk
    float charge_current_max = 20;  // A        PCB maximum: 20 A

    // State: CV/absorption
    float cell_voltage_max = 2.4;        // max voltage per cell
    int time_limit_CV = 120*60; // sec
    float current_cutoff_CV = 2.0; // A

    // State: float/trickle
    bool trickle_enabled = true;
    float cell_voltage_trickle = 2.3;    // target voltage for trickle charging of lead-acid batteries
    int time_trickle_recharge = 30*60;     // sec

    // State: equalization
    bool equalization_enabled = false;
    float cell_voltage_equalization = 2.5; // V
    int time_limit_equalization = 60*60; // sec
    float current_limit_equalization = 1.0; // A
    int equalization_trigger_time = 8; // weeks
    int equalization_trigger_deep_cycles = 10; // times

    float cell_voltage_load_disconnect = 1.95;
    float cell_voltage_load_reconnect = 2.2;

    // TODO
    float temperature_compensation = 1.0;
};

// possible charger states
enum charger_states {CHG_IDLE, CHG_CC, CHG_CV, CHG_TRICKLE, CHG_EQUALIZATION};


/** Create Charge Controller object
 *
 *  @param profile ChargingProfile struct including voltage limits, etc.
 */
void charger_init(ChargingProfile *profile);

/** Get target battery current for current charger state
 *
 *  @returns
 *    Target current (A)
 */
float charger_read_target_current();

/** Get target battery voltage for current charger state
 *
 *  @returns
 *    Target voltage (V)
 */
float charger_read_target_voltage();

/** Determine if charging of the battery is allowed
 *
 *  @returns
 *    True if charging is allowed
 */
bool charger_charging_enabled();

/** Determine if discharging the battery is allowed
 *
 *  @returns
 *    True if discharging is allowed
 */
bool charger_discharging_enabled();

/** Charger state machine update, should be called exactly once per second
 *
 *  @param battery_voltage Actual measured battery voltage (V)
 *  @param battery_current Actual measured battery current (A)
 */
void charger_update(float battery_voltage, float battery_current);

/** Get current charge controller state
 *
 *  @returns
 *    CHG_IDLE, CHG_CC, CHG_CV, CHG_TRICKLE or CHG_EQUALIZATION
 */
int charger_get_state();


#endif // CHARGER_H
