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

#ifndef BAT_CHARGER_H
#define BAT_CHARGER_H

/** @file
 *
 * @brief Battery and charger configuration and functions
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "dcdc.h"
#include "dc_bus.h"

/** Battery cell types
 */
enum BatType {
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

} BatConf;

/** Charger configuration and battery state
 */
typedef struct
{
    unsigned int state;             ///< Current charger state (see enum ChargerState)

    int num_batteries;              ///< Used for automatic 12V/24V battery detection at start-up (can be 1 or 2 only)

    float bat_temperature;          ///< Battery temperature in °C from ext. temperature sensor (if existing)
    bool ext_temp_sensor;           ///< True if external temperature sensor was detected

    float usable_capacity;          ///< Estimated usable capacity (Ah) based on coulomb counting

    float discharged_Ah;            ///< Coulomb counter for SOH calculation

    uint16_t num_full_charges;      ///< Number of full charge cycles
    uint16_t num_deep_discharges;   ///< Number of deep-discharge cycles

    uint16_t soc;                   ///< State of Charge (%)
    uint16_t soh;                   ///< State of Health (%)

    int time_state_changed;         ///< Timestamp of last state change
    int time_voltage_limit_reached; ///< Last time the CV limit was reached

    bool full;                      ///< Flag to indicate if battery was fully charged
    bool first_full_charge_reached; ///< Set to true if battery was fully charged at least once (necessary for proper capacity estimation)

} Charger;


/** Possible charger states
 *
 * Further information:
 * - https://en.wikipedia.org/wiki/IUoU_battery_charging
 * - https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
 */
enum ChargerState {

    /** Idle
     *
     * Initial state of the charge controller. If the solar voltage is high enough
     * and the battery is not full, bulk charging mode is started.
     */
    CHG_STATE_IDLE,

    /** Bulk / CC / MPPT charging
     *
     * The battery is charged with maximum possible current (MPPT algorithm is
     * active) until the CV voltage limit is reached.
     */
    CHG_STATE_BULK,

    /** Topping / CV / absorption charging
     *
     * Lead-acid batteries are charged for some time using a slightly higher charge
     * voltage. After a current cutoff limit or a time limit is reached, the charger
     * goes into trickle or equalization mode for lead-acid batteries or back into
     * Standby for Li-ion batteries.
     */
    CHG_STATE_TOPPING,

    /** Trickle charging
     *
     * This mode is kept forever for a lead-acid battery and keeps the battery at
     * full state of charge. If too much power is drawn from the battery, the
     * charger switches back into CC / bulk charging mode.
     */
    CHG_STATE_TRICKLE,

    /** Equalization charging
     *
     * This mode is only used for lead-acid batteries after several deep-discharge
     * cycles or a very long period of time with no equalization. Voltage is
     * increased to 15V or above, so care must be taken for the other system
     * components attached to the battery. (currently, no equalization charging is
     * enabled in the software)
     */
    CHG_STATE_EQUALIZATION
};

/** Basic initialization of battery configuration
 *
 * Configures battery based on settings defined in config.h and initializes
 * bat_user with same values
 */
void battery_conf_init(BatConf *bat, BatType type, int num_cells, float nominal_capacity);

/** Checks battery user settings for plausibility
 */
bool battery_conf_check(BatConf *bat);

/** Overwrites battery settings (config should be checked first)
 *
 * Settings specified in bat_user will be copied to actual battery_t,
 * if suggested updates are valid (includes plausibility check!)
 */
void battery_conf_overwrite(BatConf *source, BatConf *destination, Charger *charger = NULL);

/** SOC estimation
 *
 * Must be called exactly once per second, otherwise SOC calculation gets wrong.
 */
void battery_update_soc(BatConf *bat_conf, Charger *charger, DcBus *bus);

/** Initialize dc bus for battery connection
 *
 * @param num_batteries definies the number of series connected batteries, e.g. 2 for 24V system
 */
void battery_init_dc_bus(DcBus *bus, BatConf *bat, unsigned int num_batteries);

/** Basic initialization of charger struct
 */
void charger_init(Charger *charger);

/** Detect if two batteries are connected in series (12V/24V auto-detection)
 */
void charger_detect_num_batteries(Charger *charger, BatConf *bat, DcBus *bus);

/** Charger state machine update, should be called once per second
 */
void charger_state_machine(DcBus *port, BatConf *bat_conf, Charger *charger);

#endif /* BAT_CHARGER_H */
