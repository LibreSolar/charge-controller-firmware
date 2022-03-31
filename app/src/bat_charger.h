/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BAT_CHARGER_H
#define BAT_CHARGER_H

/** @file
 *
 * @brief Battery and charger configuration and functions
 */

#include "kalman_soc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "power_port.h"


#define CHARGER_TIME_NEVER INT32_MIN

//#define DEBUG 0
#define Nsta_SoC           3 ///<  number of states
#define Nobs_SoC           1 ///<  number of observables

/**
 * Extended Kalman Filter (EKF) configuration data
 *
 * Data will be initialized in Charger::init_terminal
 */
typedef struct
{
    /**
     * number of state values
     *
     */
    int n;

    /**
     * number of observables
     *
     */
    int m;

    /**
     * state vector
     *
     * [ir0 hk0 SOC0]
     */
    float x[Nsta_SoC];

    /**
     * prediction error covariance
     *
     */
    float P[Nsta_SoC][Nsta_SoC];

    /**
     * process noise covariance
     *
     * uncertainty of current sensor, state equation defined as fx
     */
    float Q[Nsta_SoC][Nsta_SoC];

    /**
     *  measurement error covariance
     *
     * uncertainty of voltage sensor, output equation defined as hx
     */
    float R[Nobs_SoC][Nobs_SoC];

    /**
     * Kalman gain; a.k.a. K
     *
     */
    float G[Nsta_SoC][Nobs_SoC];

    /**
     * Jacobian of process model
     *
     */
    float F[Nsta_SoC][Nsta_SoC];

    /**
     * Jacobian of measurement model
     *
     */
    float H[Nobs_SoC][Nsta_SoC];

    /**
     *  transpose of measurement Jacobian
     *
     */
    float Ht[Nsta_SoC][Nobs_SoC];

    /**
     * transpose of process Jacobian
     *
     */
    float Ft[Nsta_SoC][Nsta_SoC];

    /**
     *  P, post-prediction, pre-update
     *
     */
    float Pp[Nsta_SoC][Nsta_SoC];

    /**
     * output of user defined f() state-transition function
     *
     */
    float fx[Nsta_SoC];

    /**
     * output of user defined h() measurement function
     *
     */
    float hx[Nobs_SoC];

    /**
     * temporary storage
     *
     */
    float tmp0[Nsta_SoC][Nsta_SoC];
    float tmp1[Nsta_SoC][Nobs_SoC];
    float tmp2[Nobs_SoC][Nsta_SoC];
    float tmp3[Nobs_SoC][Nobs_SoC];
    float tmp4[Nobs_SoC][Nobs_SoC];
    float tmp5[Nobs_SoC];

} EKF_SOC;

/**
 * Clamp function to ensure SoC within min max boders.
 *
 * @param value SoC value for which to ensure to be within borders min, max
 * @param min The minimum value 0% the SoC is allowed to have
 * @param max The maximum value 100% the SoC is allowed to have
 * @return float Either the handed value or 0%, 100$ when excisting borders.
 */
float clamp(float value, float min, float max);

/**
 * Calculates initial SoC based on handed voltage
 *
 * @param batteryVoltagemV
 * @return float Inital SoC
 */

float calculateInitialSoC(float batteryVoltagemV);

/**
 * Initializes Extended Kalman Filter matrices based on handed parameters
 * 
 * *
 * WARNING TODO: It is unclear if the equations used are correct, thus the init of F might be wrong
 *
 * @param ekf_SoC struct in which initialized matrices are stored
 * @param v0 initial voltage on which the inital SoC is based
 * @param P0 Diagonal values for P matrix
 * @param Q0 Diagonal values for Q matrix
 * @param R0 Diagonal values for R matrix
 * @param initialSoC inital SoC if known.
 */
void init_soc(EKF_SOC *ekf_SoC, float v0, float P0, float Q0, float R0, float initialSoC);

/**
 * Unites f and h function and forms the complete battery model
 *
 * @param ekf_SoC struct with EKF Parameters
 * @param isBatteryInFloat for Lead Acid Batterys Float Charging Status
 * @param batteryEff Efficieny currently not used
 * @param batteryCurrentmA
 * @param samplePeriodMilliSec Period between battery current and voltage measurements
 * @param batteryCapacityAh
 * @return float Efficiency currently not used
 */
float model_soc(EKF_SOC *ekf_SoC, bool isBatteryInFloat, float batteryEff, float batteryCurrentmA,
                float samplePeriodMilliSec, float batteryCapacityAh);

/**
 * project the state of charge ahead one step using a Coulomb counting model
 * (Integration of the current over time)
 * x{k+1}(indexSOC) = x{k} - \frac{1}{{Q_{C}}}\int_{0}^{\Delta t} {i(t)\ dt}
 *
 * @param ekf_SoC  struct with EKF Parameters
 * @param isBatteryInFloat Is the lead acid batterie in float charge?
 * @param batteryEff Efficiency currently not used
 * @param batteryCurrentmA
 * @param samplePeriodMilliSec Period between battery measurements
 * @param batteryCapacity
 * @return float
 */
float f(EKF_SOC *ekf_SoC, bool isBatteryInFloat, float batteryEff, float batteryCurrentmA,
        float samplePeriodMilliSec, float batteryCapacity);

/**
 * predict the measurable value (voltage) ahead one step using the newly estimated state of charge
 * {h}(k) = {OCV}(x{k}) - {R}_{0} i(t) - {R}_{1} {i}_{R_1}(t) in mV
 *
 * WARNING TODO: It is unclear if the equations used are correct and if so if their implementation
 * is correct thus x vector and H Matrix might be wrong.
 *
 * @param ekf_SoC  struct with EKF Parameters
 * @param batteryCurrentmA
 */
void h(EKF_SOC *ekf_SoC, float batteryCurrentmA);

/**
 * Battery cell types
 */
enum BatType
{
    /*
     * IMPORTANT: make sure to adjust also boards/Kconfig if enum is changed here!
     */
    BAT_TYPE_CUSTOM = 0, ///< Custom battery type
    BAT_TYPE_FLOODED,    ///< Old flooded (wet) lead-acid batteries
    BAT_TYPE_GEL,        ///< VRLA gel batteries (maintainance-free)
    BAT_TYPE_AGM,        ///< AGM batteries (maintainance-free)
    BAT_TYPE_LFP,        ///< LiFePO4 Li-ion batteries (3.3V nominal)
    BAT_TYPE_NMC,        ///< NMC/Graphite Li-ion batteries (3.7V nominal)
    BAT_TYPE_NMC_HV,     ///< NMC/Graphite High Voltage Li-ion batteries (3.7V nominal, 4.35 max)
};

/**
 * Battery configuration data
 *
 * Data will be initialized in battery_init depending on configured cell type in config.h.
 */
typedef struct
{
    /**
     * Nominal battery capacity or sum of parallel cells capacity (Ah)
     *
     * The nominal capacity is used for SOC calculation and definition of current limits.
     */
    float nominal_capacity;

    /**
     * Recharge voltage (V)
     *
     * Start charging again if voltage of fully charged battery drops below this threshold.
     *
     * Remark: Setting the value too close to the max voltage will cause more charging stress on
     * lithium based batteries.
     */
    float recharge_voltage;

    /**
     * Recharge time limit (sec)
     *
     * Start charging of previously fully charged battery earliest after this period of time.
     */
    uint32_t time_limit_recharge;

    /**
     * Absolute maximum voltage (V)
     *
     * Above this voltage the battery or loads might get damaged.
     */
    float absolute_max_voltage;

    /**
     * Absolute minimum voltage (V)
     *
     * Below this voltage the battery is considered damaged.
     */
    float absolute_min_voltage;

    /**
     * Maximum charge current in *CC/bulk* phase (A)
     *
     * Limits the current if the PCB allows more than the battery. (positive value)
     */
    float charge_current_max;

    /**
     * Maximum discharge current via load port (A)
     *
     * Limits the current if the PCB allows more than the battery. (positive value)
     */
    float discharge_current_max;

    /**
     * Maximum voltage in CV/absorption phase (V)
     *
     * Charger target voltage, switching from CC to CV at this voltage threshold.
     */
    float topping_voltage;

    /**
     * CV phase cut-off current limit (A)
     *
     * Constant voltage charging phase stopped if current is below this value.
     */
    float topping_cutoff_current;

    /**
     * CV phase cut-off time limit (s)
     *
     * After this time, CV charging is stopped independent of current.
     */
    uint32_t topping_duration;

    /**
     * Enable float/trickle charging
     *
     * Caution: Do not enable float charging for lithium-ion batteries
     */
    bool float_enabled;

    /**
     * Float voltage (V)
     *
     * Charger target voltage for float charging of lead-acid batteries
     */
    float float_voltage;

    /**
     * Float recharge time (s)
     *
     * If the float voltage is not reached anymore (e.g. because of lack of solar
     * input power) for this period of time, the charger state machine goes back
     * into CC/bulk charging mode.
     */
    uint32_t float_recharge_time;

    /**
     * Enable equalization charging
     *
     * Caution: Do not enable float charging for lithium-ion batteries
     */
    bool equalization_enabled;

    /**
     * Equalization voltage (V)
     *
     * Charger target voltage for equalization charging of lead-acid batteries
     */
    float equalization_voltage;

    /**
     * Equalization cut-off time limit (s)
     *
     * After this time, equalization charging is stopped.
     */
    uint32_t equalization_duration;

    /**
     * Equalization phase maximum current (A)
     *
     * The current of the charger is controlled to stay below this limit during equalization.
     */
    float equalization_current_limit;

    /**
     * Equalization trigger time interval (days)
     *
     * After passing specified amount of days, an equalization charge is triggered.
     */
    uint32_t equalization_trigger_days;

    /**
     * Equalization trigger deep-discharge cycles
     *
     * After specified number of deep discharges, an equalization charge is triggered.
     */
    uint32_t equalization_trigger_deep_cycles;

    /**
     * Load disconnect open circuit voltage (V)
     *
     * Load output is disabled if battery voltage is below this threshold.
     *
     * The voltage is current-compensated using the battery internal resistance:
     * threshold = load_disconnect_voltage + battery_current * internal_resistance
     *
     * (Remark: battery_current negative during discharge, internal_resistance positive)
     */
    float load_disconnect_voltage;

    /**
     * Load reconnect open circuit voltage (V)
     *
     * Re-enable the load only after charged beyond this value.
     *
     * The voltage is current-compensated using the battery internal resistance:
     * threshold = load_reconnect_voltage + battery_current * internal_resistance
     *
     * (Remark: battery_current positive during charge, internal_resistance positive)
     */
    float load_reconnect_voltage;

    /**
     * Battery internal resistance (Ohm)
     *
     * Resistance value for current-compensation of load switch voltage thresholds.
     *
     * The value must have a positive sign. Only resistance values leading to less than 10% voltage
     * drop at max. current are allowed.
     */
    float internal_resistance;

    /**
     * Resistance of wire between charge controller and battery (Ohm)
     *
     * Resistance value for current-compensation of charging voltages.
     *
     * The value must have a positive sign. Only resistance values leading to less than 3% voltage
     * loss at max. current are allowed.
     */
    float wire_resistance;

    /**
     * Open circuit voltage of full battery (V)
     *
     * Used for simple state of charge (SOC) algorithm.
     */
    float ocv_full;

    /**
     * Open circuit voltage of empty battery (V)
     *
     * Used for simple state of charge (SOC) algorithm.
     */
    float ocv_empty;

    /**
     * Maximum allowed charging temperature of the battery (°C)
     */
    float charge_temp_max;

    /**
     * Minimum allowed charging temperature of the battery (°C)
     */
    float charge_temp_min;

    /**
     * Maximum allowed discharging temperature of the battery (°C)
     */
    float discharge_temp_max;

    /**
     * Minimum allowed discharging temperature of the battery (°C)
     */
    float discharge_temp_min;

    /**
     * Voltage compensation based on battery temperature (mV/K/cell)
     *
     * Suggested value: -3 mV/K/cell
     */
    float temperature_compensation;

} BatConf;

/**
 * Possible charger states
 *
 * Further information:
 * - https://en.wikipedia.org/wiki/IUoU_battery_charging
 * - https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery
 */
enum ChargerState
{
    /**
     * Idle
     *
     * Initial state of the charge controller. If the solar voltage is high enough
     * and the battery is not full, bulk charging mode is started.
     */
    CHG_STATE_IDLE,

    /**
     * Bulk / CC / MPPT charging
     *
     * The battery is charged with maximum possible current (MPPT algorithm is
     * active) until the CV voltage limit is reached.
     */
    CHG_STATE_BULK,

    /**
     * Topping / CV / absorption charging
     *
     * Lead-acid batteries are charged for some time using a slightly higher charge
     * voltage. After a current cutoff limit or a time limit is reached, the charger
     * goes into float or equalization mode for lead-acid batteries or back into
     * Standby for Li-ion batteries.
     */
    CHG_STATE_TOPPING,

    /**
     * Float / trickle charging
     *
     * This mode is kept forever for a lead-acid battery and keeps the battery at
     * full state of charge. If too much power is drawn from the battery, the
     * charger switches back into CC / bulk charging mode.
     */
    CHG_STATE_FLOAT,

    /**
     * Equalization charging
     *
     * This mode is only used for lead-acid batteries after several deep-discharge
     * cycles or a very long period of time with no equalization. Voltage is
     * increased to 15V or above, so care must be taken for the other system
     * components attached to the battery. (currently, no equalization charging is
     * enabled in the software)
     */
    CHG_STATE_EQUALIZATION,

    /**
     * Parallel operation of multiple converters
     *
     * This mode is enabled if the device receives commands from an external controller with
     * higher priority on the CAN bus. The device goes in current control modes and tries to
     * match the current of the other controller.
     */
    CHG_STATE_FOLLOWER,
};

/**
 * Charger configuration and battery state
 */
class Charger
{
public:
    Charger(PowerPort *pwr_port) : port(pwr_port){};

    PowerPort *port;

    /**
     * Current charger state (see enum ChargerState)
     */
    uint32_t state = CHG_STATE_IDLE;

    /**
     * Battery temperature (°C) from ext. temperature sensor (if existing)
     */
    float bat_temperature = 25;

    /**
     * Flag to indicate if external temperature sensor was detected
     */
    bool ext_temp_sensor;

    /**
     * Estimated usable capacity (Ah) based on coulomb counting
     */
    float usable_capacity;

    /**
     * Coulomb counter for SOH calculation
     */
    float discharged_Ah;

    /**
     * Number of full charge cycles
     */
    uint16_t num_full_charges;

    /**
     * Number of deep-discharge cycles
     */
    uint16_t num_deep_discharges;

    /**
     * State of Charge (%)
     */
    uint16_t soc = 100;

    /**
     * State of Health (%)
     */
    uint16_t soh = 100;

    /**
     * Timestamp of last state change
     */
    time_t time_state_changed = CHARGER_TIME_NEVER;

    /**
     * Last time the CV limit was reached
     */
    time_t time_target_voltage_reached = CHARGER_TIME_NEVER;

    /**
     * Counts the number of seconds during which the target voltage of current charging phase was
     * reached.
     */
    uint32_t target_voltage_timer;

    /**
     * Timestamp after finish of last equalization charge
     */
    time_t time_last_equalization = CHARGER_TIME_NEVER;

    /**
     * Deep discharge counter value after last equalization
     */
    uint32_t deep_dis_last_equalization;

    /**
     * Flag to indicate if battery was fully charged
     */
    bool full;

    /**
     * Flag to indicate if battery was completely discharged
     */
    bool empty;

    /**
     * Last time a control message from external device was received
     */
    time_t time_last_ctrl_msg = CHARGER_TIME_NEVER;

    /**
     * Target current of the converter if operating in follower mode (received from external
     * converter with higher priority via CAN)
     */
    float target_current_control;

    /**
     * Detect if two batteries are connected in series (12V/24V auto-detection)
     */
    void detect_num_batteries(BatConf *bat) const;

    /**
     * Discharging control update (for load output), should be called once per second
     */
    void discharge_control(BatConf *bat_conf);

    /**
     * Charger state machine update, should be called once per second
     */
    void charge_control(BatConf *bat_conf);

    /**
     * SOC estimation
     *
     * Must be called exactly once per second, otherwise SOC calculation gets wrong.
     */
    void update_soc(BatConf *bat_conf, EKF_SOC *ekf_SoC);
    
    /**
     * SOC estimation
     *
     * WARNING: TODO obsolte function, replaced by void charge_control(BatConf *bat_conf);
     */
    void update_soc_voltage_based(BatConf *bat_conf);

    /**
     * Initialize terminal and dc bus for battery connection
     *
     * @param bat Configuration to be used for terminal setpoints
     */
    void init_terminal(BatConf *bat, EKF_SOC *ekf_SoC) const;

private:
    void enter_state(int next_state);
};

/**
 * Basic initialization of battery configuration
 *
 * Configures battery based on settings defined in config.h and initializes
 * bat_user with same values
 *
 * @param bat Battery configuration that should be initialized
 * @param type Battery type, one of enum BatType (declared as int instead of enum BatType because
 *             configuration via Kconfig symbol cannot handle enums)
 * @param num_cells Number of cells (e.g. 6 for 12V lead-acid battery)
 * @param nominal_capacity Nominal capacity (Ah)
 */
void battery_conf_init(BatConf *bat, int type, int num_cells, float nominal_capacity);

/**
 * Checks battery user settings for plausibility
 */
bool battery_conf_check(BatConf *bat);

/**
 * Overwrites battery settings (config should be checked first)
 *
 * Settings specified in bat_user will be copied to actual battery_t,
 * if suggested updates are valid (includes plausibility check!)
 */
void battery_conf_overwrite(BatConf *source, BatConf *destination, Charger *charger = NULL);

/**
 * Checks if incoming configuration is different to current configuration
 *
 * Returns true if changed
 */
bool battery_conf_changed(BatConf *a, BatConf *b);

#endif /* BAT_CHARGER_H */
