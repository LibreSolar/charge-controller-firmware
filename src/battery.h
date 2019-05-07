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

#ifndef BATTERY_H
#define BATTERY_H

/** @file
 *
 * @brief Battery configuration and functions
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/** Battery cell types
 */
enum bat_type {
    BAT_TYPE_NONE = 0,      ///< Safe standard settings
    BAT_TYPE_FLOODED,       ///< Old flooded (wet) lead-acid batteries
    BAT_TYPE_GEL,           ///< VRLA gel batteries (maintainance-free)
    BAT_TYPE_AGM,           ///< AGM batteries (maintainance-free)
    BAT_TYPE_LFP,           ///< LiFePO4 Li-ion batteries (3.3V nominal)
    BAT_TYPE_NMC,           ///< NMC/Graphite Li-ion batteries (3.7V nominal)
    BAT_TYPE_NMC_HV         ///< NMC/Graphite High Voltage Li-ion batteries (3.7V nominal, 4.35 max)
};

/** Battery configuration data
 *
 * Data will be initialized in battery_init depending on configured cell type in config.h.
 */
typedef struct
{
    /** Nominal battery capacity or sum of parallel cells capacity (Ah)
     *
     * The nominal capacity is used for SOC calculation and definition of current limits.
     */
    float nominal_capacity;

    /** Recharge voltage (V)
     *
     * Start charging again if voltage of fully charged battery drops below this threshold.
     *
     * Remark: Setting the value too close to the max voltage will cause more charging stress on lithium based batteries.
     */
    float voltage_recharge;

    /** Recharge time limit (sec)
     *
     * Start charging of previously fully charged battery earliest after this period of time.
     */
    int time_limit_recharge;

    /** Absolute maximum voltage (V)
     *
     * Above this voltage the battery or loads might get damaged.
     */
    float voltage_absolute_max;

    /** Absolute minimum voltage (V)
     *
     * Below this voltage the battery is considered damaged.
     */
    float voltage_absolute_min;

    /** Maximum charge current in *CC/bulk* phase (A)
     *
     * Limits the current if the PCB allows more than the battery.
     */
    float charge_current_max;

    /** Maximum voltage in CV/absorption phase (V)
     *
     * Charger target voltage, switching from CC to CV at this voltage threshold.
     */
    float voltage_topping;

    /** CV phase cut-off current limit (A)
     *
     * Constant voltage charging phase stopped if current is below this value.
     */
    float current_cutoff_topping;

    /** CV phase cut-off time limit (s)
     *
     * After this time, CV charging is stopped independent of current.
     */
    int time_limit_topping;

    /** Enable float/trickle charging
     *
     * Caution: Do not enable trickle charging for lithium-ion batteries
     */
    bool trickle_enabled;

    /** Trickle voltage (V)
     *
     * Charger target voltage for trickle charging of lead-acid batteries
     */
    float voltage_trickle;

    /** Trickle recharge time (s)
     *
     * If the trickle voltage is not reached anymore (e.g. because of lack of solar
     * input power) for this period of time, the charger state machine goes back
     * into CC/bulk charging mode.
     */
    int time_trickle_recharge;

    /** Enable equalization charging
     *
     * Caution: Do not enable trickle charging for lithium-ion batteries
     */
    bool equalization_enabled;

    /** Equalization voltage (V)
     *
     * Charger target voltage for equalization charging of lead-acid batteries
     */
    float voltage_equalization;

    /** Equalization phase cut-off time limit (s)
     *
     * After this time, CV charging is stopped independent of current.
     */
    int time_limit_equalization;

    /** Equalization phase cut-off time limit (s)
     *
     * After this time, CV charging is stopped independent of current.
     */
    float current_limit_equalization;

    /** Equalization trigger time (weeks)
     *
     * After passing specified amount of weeks, an equalization charge is triggered.
     */
    int equalization_trigger_time;

    /** Equalization trigger deep-discharge cycles
     *
     * After specified number of deep discharges, an equalization charge is triggered.
     */
    int equalization_trigger_deep_cycles;

    /** Load disconnect open circuit voltage (V)
     *
     * Load output is disabled if battery voltage is below this threshold.
     *
     * The voltage is current-compensated using the battery internal resistance:
     * threshold = voltage_load_disconnect + battery_current * internal_resistance
     *
     * (Remark: battery_current negative during discharge, internal_resistance positive)
     */
    float voltage_load_disconnect;

    /** Load reconnect open circuit voltage (V)
     *
     * Re-enable the load only after charged beyond this value.
     *
     * The voltage is current-compensated using the battery internal resistance:
     * threshold = voltage_load_reconnect + battery_current * internal_resistance
     *
     * (Remark: battery_current positive during charge, internal_resistance positive)
     */
    float voltage_load_reconnect;

    /** Battery internal resistance (Ohm)
     *
     * Resistance value for current-compensation of load switch voltage thresholds.
     *
     * The value must have a positive sign. Only resistance values leading to less than 10% voltage drop at max. current are allowed.
     */
    float internal_resistance;

    /** Resistance of wire between charge controller and battery (Ohm)
     *
     * Resistance value for current-compensation of charging voltages.
     *
     * The value must have a positive sign. Only resistance values leading to less than 3% voltage loss at max. current are allowed.
     */
    float wire_resistance;

    // used to calculate state of charge information
    float ocv_full;
    float ocv_empty;

    /** Maximum allowed charging temperature of the battery (°C)
     */
    float charge_temp_max;

    /** Minimum allowed charging temperature of the battery (°C)
     */
    float charge_temp_min;

    /** Maximum allowed discharging temperature of the battery (°C)
     */
    float discharge_temp_max;

    /** Minimum allowed discharging temperature of the battery (°C)
     */
    float discharge_temp_min;

    /** Voltage compensation based on battery temperature (mV/K/cell)
     *
     * Suggested value: -3 mV/K/cell
     */
    float temperature_compensation;

} battery_conf_t;

typedef struct
{
    int num_batteries;      ///< Used for automatic 12V/24V battery detection at start-up (can be 1 or 2 only)

    float temperature;      ///< Battery temperature in °C from ext. temperature sensor (if existing)
    bool ext_temp_sensor;   ///< True if external temperature sensor was detected

    float usable_capacity;      ///< Estimated usable capacity (Ah) based on coulomb counting

    float discharged_Ah;        ///< Coulomb counter for SOH calculation

    uint16_t num_full_charges;      ///< Number of full charge cycles
    uint16_t num_deep_discharges;   ///< Number of deep-discharge cycles

    uint16_t soc;                   ///< State of Charge (%)
    uint16_t soh;                   ///< State of Health (%)
    unsigned int chg_state;            ///< Current charger state (see enum charger_states)
    int time_state_changed;            ///< Timestamp of last state change
    int time_voltage_limit_reached;    ///< Last time the CV limit was reached

    bool full;              ///< Flag to indicate if battery was fully charged

} battery_state_t;


/** Basic initialization of battery configuration
 *
 * Configures battery based on settings defined in config.h and initializes
 * bat_user with same values
 */
void battery_conf_init(battery_conf_t *bat, bat_type type, int num_cells, float nominal_capacity);

/** Checks battery user settings
 *
 * This function should be implemented in config.cpp
 */
bool battery_conf_check(battery_conf_t *bat);

/** Overwrites battery settings (config should be checked first)
 *
 * Settings specified in bat_user will be copied to actual battery_t,
 * if suggested updates are valid (includes plausibility check!)
 */
void battery_conf_overwrite(battery_conf_t *source, battery_conf_t *destination, battery_state_t *state = NULL);

/** Basic initialization of battery state (e.g. SOC)
 */
void battery_state_init(battery_state_t *bat_state);

/** SOC estimation
 *
 * Must be called exactly once per second, otherwise SOC calculation gets wrong.
 */
void battery_update_soc(battery_conf_t *bat_conf, battery_state_t *bat_state, float voltage, float current);

#endif /* BATTERY_H */
