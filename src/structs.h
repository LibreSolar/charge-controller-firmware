
#ifndef __STRUCTS_H_
#define __STRUCTS_H_

#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


/* ToDo: change to int values
 * Power:   W/10 = 100 mW   --> int16_t max. 3276.8 W
 * Current: W/100 = 10 mA   --> int16_t max. 327.68 A
 * Voltage: V/100 = 10 mV   --> int16_t max. 327.68 V
 */

typedef struct
{
    // configuration data (read any time, write only at startup / init, change requires reinit of charger )
    int num_cells;
    float capacity;                   // Ah

    // State: Standby
    int time_limit_recharge;         // sec
    float cell_voltage_recharge;     // V

    float cell_voltage_absolute_min; // V   (below this voltage the battery is considered damaged)
    float cell_voltage_absolute_max; // V   (above this voltage the battery can be damaged)

    // State: CC/bulk
    float charge_current_max;        // A

    // State: CV/absorption
    float cell_voltage_max;        // max voltage per cell
    int time_limit_CV;             // sec
    float current_cutoff_CV;       // A

    // State: float/trickle
    bool trickle_enabled;
    float cell_voltage_trickle;    // target voltage for trickle charging of lead-acid batteries
    int time_trickle_recharge;     // sec

    // State: equalization
    bool equalization_enabled;
    float cell_voltage_equalization;      // V
    int time_limit_equalization;          // sec
    float current_limit_equalization;     // A
    int equalization_trigger_time;        // weeks
    int equalization_trigger_deep_cycles; // times


    float cell_voltage_load_disconnect;
    float cell_voltage_load_reconnect;

    // used to calculate state of charge information 
    float cell_ocv_full;
    float cell_ocv_empty;

    float temperature_compensation;     // voltage compensation (suggested: -3 mV/°C/cell)

    bool valid_config;

    // operational data below (r/w during operation)

    float temperature;                  // °C from ext. temperature sensor (if existing)
    
    float input_Wh_day;
    float output_Wh_day;
    float input_Wh_total;
    float output_Wh_total;

    int num_full_charges;
    int num_deep_discharges;
    
    int soc;
    int state;                         // valid states: enum charger_states
    int time_state_changed;            // timestamp of last state change
    int time_voltage_limit_reached;    // last time the CV limit was reached
    
    bool full;
} battery_t;

// battery cell types 
enum cell_type {
    CELL_TYPE_NONE = 0,        // stafe standard settings
    CELL_TYPE_FLOODED,         // old flooded (wet) lead-acid batteries
    CELL_TYPE_GEL,             // VRLA gel batteries (maintainance-free)
    CELL_TYPE_AGM,             // AGM batteries (maintainance-free)
    CELL_TYPE_LFP,             // LiFePO4 Li-ion batteries (3.3V nominal)
    CELL_TYPE_NMC,             // NMC/Graphite Li-ion batteries (3.7V nominal)
    CELL_TYPE_NMC_HV,          // NMC/Graphite High Voltage Li-ion batteries (3.7V nominal, 4.35 max)
};

// possible charger states
enum charger_state {
    CHG_STATE_IDLE, 
    CHG_STATE_CC, 
    CHG_STATE_CV, 
    CHG_STATE_TRICKLE, 
    CHG_STATE_EQUALIZATION
};

/* DC/DC port type
 * 
 * Saves current target settings of either high-side or low-side port of a
 * DC/DC converter. In this way, e.g. a battery can be configured to be
 * connected to high or low side of DC/DC converter w/o having to rewrite
 * the control algorithm.
 */
typedef struct {
    // actual measurements
    float voltage;
    float current;

    // target voltage if port is configured as output
    float voltage_output_target;
    float droop_resistance;     // v_target = v_out_max - r_droop * current
    
    // minimum voltage to allow current output (necessary to prevent charging of deep-discharged Li-ion batteries)
    float voltage_output_min;

    // minimum voltage to allow current input (= discharging of batteries)
    float voltage_input_target;  // = starting point for discharging of batteries
    float voltage_input_stop;    // = absolute minimum

    float current_output_max;   // for battery charging
    float current_input_max;
    
    bool output_allowed;        // batteries: charging
    bool input_allowed;         // batteries: discharging
} dcdc_port_t;

/* DC/DC type
 * 
 * Contains all data belonging to the DC/DC sub-component of the PCB, incl.
 * actual measurements and calibration parameters.
 */
typedef struct {
    // actual measurements
    float ls_current;           // inductor current
    float temp_mosfets;

    // current state
    float power;                // power at low-side (calculated by dcdc controller)
    int pwm_delta;              // direction of PWM change for MPPT
    int off_timestamp;          // time when DC/DC was switched off last time

    // maximum allowed values
    float ls_current_max;       // PCB inductor maximum
    float ls_current_min;       // --> if lower, charger is switched off
    float hs_voltage_max;
    float ls_voltage_max;

    // calibration parameters
    //float offset_voltage_start;  // V  charging switched on if Vsolar > Vbat + offset
    //float offset_voltage_stop;   // V  charging switched off if Vsolar < Vbat + offset
    int restart_interval;   // s    --> when should we retry to start charging after low solar power cut-off?
} dcdc_t;

/* DC/DC basic operation mode
 * 
 * Defines which type of device is connected to the high side and low side ports
 */
enum dcdc_control_mode
{
    MODE_MPPT_BUCK,     // solar panel at high side port, battery / load at low side port (typical MPPT)
    MODE_MPPT_BOOST,    // battery at high side port, solar panel at low side (e.g. e-bike charging)
    MODE_NANOGRID       // accept input power (if available and need for charging) or provide output power 
                        // (if no other power source on the grid and battery charged) on the high side port 
                        // and dis/charge battery on the low side port, battery voltage must be lower than 
                        // nano grid voltage.
};

/* Load Output type
 * 
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
typedef struct {
    float current;              // actual measurement
    float current_max;          // maximum allowed current
    bool overcurrent_flag;
    bool enabled;               // actual current setting
    bool enabled_target;        // target setting defined via communication 
                                // port (overruled if battery is empty)
    bool usb_enabled_target;    // same for USB output
} load_output_t;

/* Log Data
 *
 * Saves maximum ever measured values to EEPROM
 */
typedef struct {
    float battery_voltage_max;
    float solar_voltage_max;
    float dcdc_current_max;
    float load_current_max;
    float temp_int;             // °C (internal MCU temperature sensor)
    float temp_int_max;         // °C
    float temp_mosfets_max;
} log_data_t;


#ifdef __cplusplus
}
#endif

struct BatteryConfigUser
{
    BatteryConfigUser();

    enum cell_type type;  // mandatory, is used to determine charging strategy and cell voltage limits,
                            // see structs.h BatteryType for complete list

    float capacity;         // mandatory, what is the capacity of the battery (pack), unit: Ah
                            // if you have cells in parallel, add the individual capacitie
                            // value is not too critical, it is mainly used to determine charge currents.

    uint8_t num_cells;      // mandatory, how many cells are connected in series


    // all batteries will optionally use these values, leave at 0 to have these calculated automatically
    // State: CC/bulk
    float charge_current_max;           // never user more than this current for charging, unit: A
    float current_cutoff_CV;            // stop the final constant voltage charging phase if the charge current falls below this
                                        // value, unit: A

    // Lithium batteries can optionally use these values, leave at 0 to have these calculated automatically
    float cell_voltage_max;         // max charge voltage per cell, switch at this voltage from constant current (CC) to constant 
                                    // voltage charging (CV) phase. 
    float cell_voltage_recharge;    // when voltage drops below start charging after battery has been fully charged and charging 
                                    // was stopped
                                    // setting it too close to the max voltage will cause more charging stress on lithium based
                                    // batteries 

    float cell_voltage_load_disconnect; // when discharging, stop power to load measure battery voltage drops below this value
    float cell_voltage_load_reconnect;  // when charging, only start power to load if reaching this battery voltage  

};


#endif // STRUCTS_H
