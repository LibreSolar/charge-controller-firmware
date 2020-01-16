/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BAT_CHARGER_H
#define BAT_CHARGER_H

/** @file
 *
 * @brief Battery and charger configuration and functions
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "power_port.h"

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
     * Limits the current if the PCB allows more than the battery. (positive value)
     */
    float charge_current_max;

    /** Maximum discharge current via load port (A)
     *
     * Limits the current if the PCB allows more than the battery. (positive value)
     */
    float discharge_current_max;

    /** Maximum voltage in CV/absorption phase (V)
     *
     * Charger target voltage, switching from CC to CV at this voltage threshold.
     */
    float topping_voltage;

    /** CV phase cut-off current limit (A)
     *
     * Constant voltage charging phase stopped if current is below this value.
     */
    float topping_current_cutoff;

    /** CV phase cut-off time limit (s)
     *
     * After this time, CV charging is stopped independent of current.
     */
    int topping_duration;

    /** Enable float/trickle charging
     *
     * Caution: Do not enable trickle charging for lithium-ion batteries
     */
    bool trickle_enabled;

    /** Trickle voltage (V)
     *
     * Charger target voltage for trickle charging of lead-acid batteries
     */
    float trickle_voltage;

    /** Trickle recharge time (s)
     *
     * If the trickle voltage is not reached anymore (e.g. because of lack of solar
     * input power) for this period of time, the charger state machine goes back
     * into CC/bulk charging mode.
     */
    int trickle_recharge_time;

    /** Enable equalization charging
     *
     * Caution: Do not enable trickle charging for lithium-ion batteries
     */
    bool equalization_enabled;

    /** Equalization voltage (V)
     *
     * Charger target voltage for equalization charging of lead-acid batteries
     */
    float equalization_voltage;

    /** Equalization cut-off time limit (s)
     *
     * After this time, equalization charging is stopped.
     */
    int equalization_duration;

    /** Equalization phase maximum current (A)
     *
     * The current of the charger is controlled to stay below this limit during equalization.
     */
    float equalization_current_limit;

    /** Equalization trigger time interval (days)
     *
     * After passing specified amount of days, an equalization charge is triggered.
     */
    int equalization_trigger_days;

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
class Charger
{
public:
    Charger(PowerPort *pwr_port):
        port(pwr_port) {};

    PowerPort *port;

    unsigned int state;             ///< Current charger state (see enum ChargerState)

    int num_batteries = 1;          ///< Used for automatic 12V/24V battery detection at
                                    ///< start-up (can be 1 or 2 only)

    float bat_temperature = 25;     ///< Battery temperature in °C from ext. temperature sensor
                                    ///< (if existing)
    bool ext_temp_sensor;           ///< True if external temperature sensor was detected

    float usable_capacity;          ///< Estimated usable capacity (Ah) based on coulomb counting

    float discharged_Ah;            ///< Coulomb counter for SOH calculation

    uint16_t num_full_charges;      ///< Number of full charge cycles
    uint16_t num_deep_discharges;   ///< Number of deep-discharge cycles

    uint16_t soc = 100;             ///< State of Charge (%)
    uint16_t soh = 100;             ///< State of Health (%)

    time_t time_state_changed;         ///< Timestamp of last state change
    time_t time_voltage_limit_reached; ///< Last time the CV limit was reached

    time_t time_last_equalization;  ///< Timestamp after finish of last equalization charge
    int deep_dis_last_equalization; ///< Deep discharge counter value after last equalization

    bool full;                      ///< Flag to indicate if battery was fully charged
    bool first_full_charge_reached = false;
                                    //< Set to true if battery was fully charged at least once
                                    ///< (necessary for proper capacity estimation)

    /** Detect if two batteries are connected in series (12V/24V auto-detection)
     */
    void detect_num_batteries(BatConf *bat);

    /** Discharging control update (for load output), should be called once per second
     */
    void discharge_control(BatConf *bat_conf);

    /** Charger state machine update, should be called once per second
     */
    void charge_control(BatConf *bat_conf);

    /** SOC estimation
     *
     * Must be called exactly once per second, otherwise SOC calculation gets wrong.
     */
    void update_soc(BatConf *bat_conf);

private:
    void enter_state(int next_state);
};


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

/** Initialize dc bus for battery connection
 *
 * @param num_batteries definies the number of series connected batteries, e.g. 2 for 24V system
 */
void battery_init_dc_bus(PowerPort *port, BatConf *bat, unsigned int num_batteries);

#endif /* BAT_CHARGER_H */
