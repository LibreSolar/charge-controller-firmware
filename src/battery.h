
#ifndef __BATTERY_H_
#define __BATTERY_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* ToDo: change to int values
 * Power:   W/10 = 100 mW   --> int16_t max. 3276.8 W
 * Current: W/100 = 10 mA   --> int16_t max. 327.68 A
 * Voltage: V/100 = 10 mV   --> int16_t max. 327.68 V
 */

// battery types
enum bat_type {
    BAT_TYPE_NONE = 0,      // safe standard settings
    BAT_TYPE_FLOODED,       // old flooded (wet) lead-acid batteries
    BAT_TYPE_GEL,           // VRLA gel batteries (maintainance-free)
    BAT_TYPE_AGM,           // AGM batteries (maintainance-free)
    BAT_TYPE_LFP,           // LiFePO4 Li-ion batteries (3.3V nominal)
    BAT_TYPE_NMC,           // NMC/Graphite Li-ion batteries (3.7V nominal)
    BAT_TYPE_NMC_HV,        // NMC/Graphite High Voltage Li-ion batteries (3.7V nominal, 4.35 max)
    BAT_TYPE_CUSTOM         // user configurable
};

typedef struct
{
    /* Configuration data
     *
     * Data will is initialized in battery_int depending on configured cell type in config.h. It can
     * be customized by overloading the weak function battery_init_user.
     */

    // Core battery information
    bat_type type;                  // -    see above for allowed types
    unsigned int num_cells;         // -    number of cells in series
    float nominal_capacity;         // Ah   cell capacity or sum of parallel cells capacity
                                    //      Capacity is currently mainly used to define current limits, but might
                                    //      become important for improved SOC calculation algorithms
    float useable_capacity;         // Ah   estimated useable capacity based on coulomb counting

    // State: Standby
    float cell_voltage_recharge;    // V    below this voltage start charging again after battery has been fully charged
                                    //      Remark: setting the value too close to the max voltage will cause more
                                    //      charging stress on lithium based batteries
    int time_limit_recharge;        // sec

    float cell_voltage_absolute_min; // V   below this voltage the battery is considered damaged
    //float cell_voltage_absolute_max; // V   above this voltage the battery can be damaged

    // State: CC/bulk
    float charge_current_max;       // A    limits the current if PCB allows more than the battery

    // State: CV/absorption
    float cell_voltage_max;         // V    charger target voltage per cell, switching from CC to CV at this voltage
    float current_cutoff_CV;        // A    constant voltage charging phase stopped if current is below this value
    int time_limit_CV;              // sec  after this time limit, CV charging is stopped independent of current

    // State: float/trickle
    bool trickle_enabled;
    float cell_voltage_trickle;    // V     charger target voltage for trickle charging of lead-acid batteries
    int time_trickle_recharge;     // sec

    // State: equalization
    bool equalization_enabled;
    float cell_voltage_equalization;      // V
    int time_limit_equalization;          // sec
    float current_limit_equalization;     // A
    int equalization_trigger_time;        // weeks
    int equalization_trigger_deep_cycles; // times

    float cell_voltage_load_disconnect; // V    when discharging, stop power to load if battery voltage drops below this value
    float cell_voltage_load_reconnect;  // V    re-enable the load only after charged beyond this value

    // used to calculate state of charge information
    float cell_ocv_full;
    float cell_ocv_empty;

    float temperature_compensation;     // voltage compensation (suggested: -3 mV/°C/cell)

    /* Operational data (updated during operation)
     */

    int num_batteries;      // used for automatic 12V/24V battery detection at start-up
                            // can be 1 or 2 only.

    float temperature;                  // °C from ext. temperature sensor (if existing)

    float input_Wh_day;
    float output_Wh_day;
    uint32_t input_Wh_total;
    uint32_t output_Wh_total;

    float discharged_Ah;                // coloumb counter for SOH calculation

    uint16_t num_full_charges;
    uint16_t num_deep_discharges;

    uint16_t soc;
    uint16_t soh;
    unsigned int state;                // valid states: enum charger_states
    int time_state_changed;            // timestamp of last state change
    int time_voltage_limit_reached;    // last time the CV limit was reached

    bool full;

} battery_t;


// these voltages are defined on BATTERY level, NOT CELL!
typedef struct
{
    float nominal_capacity;         // Ah   cell capacity or sum of parallel cells capacity
                                    //      Capacity is currently mainly used to define current limits, but might
                                    //      become important for improved SOC calculation algorithms

    // State: Standby
    float voltage_recharge;         // V    below this voltage start charging again after battery has been fully charged
                                    //      Remark: setting the value too close to the max voltage will cause more
                                    //      charging stress on lithium based batteries

    float voltage_absolute_min;     // V    below this voltage the battery is considered damaged

    // State: CC/bulk
    float charge_current_max;       // A    limits the current if PCB allows more than the battery

    // State: CV/absorption
    float voltage_max;              // V    charger target voltage per cell, switching from CC to CV at this voltage
    float current_cutoff_CV;        // A    constant voltage charging phase stopped if current is below this value
    int time_limit_CV;              // sec  after this time limit, CV charging is stopped independent of current

    // State: float/trickle
    bool trickle_enabled;
    float voltage_trickle;         // V     charger target voltage for trickle charging of lead-acid batteries
    int time_trickle_recharge;     // sec

    float voltage_load_disconnect; // V     when discharging, stop power to load if battery voltage drops below this value
    float voltage_load_reconnect;  // V     re-enable the load only after charged beyond this value

    float temperature_compensation;     // voltage compensation (suggested: -3 mV/°C/cell)

} battery_user_settings_t;


/* Basic initialization of battery
 *
 * Configures battery based on settings defined in config.h and initializes
 * bat_user with same values
 */
void battery_init(battery_t *bat, battery_user_settings_t *bat_user);

/* User settings of battery
 *
 * This function should be implemented in config.cpp
 */
void battery_init_user(battery_t *bat, battery_user_settings_t *bat_user);

/* Update battery settings
 *
 * Settings specified in bat_user will be copied to actual battery_t,
 * if suggested updates are valid (includes plausibility check!)
 */
bool battery_user_settings(battery_t *bat, battery_user_settings_t *bat_user);

void battery_update_energy(battery_t *bat, float voltage, float current);


#ifdef __cplusplus
}
#endif

#endif // __BATTERY_H_
