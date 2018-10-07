

#ifndef DATA_OBJECTS_H
#define DATA_OBJECTS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct measurement_data_t {
    float battery_voltage;
    float battery_voltage_max;  // to be stored in EEPROM
    float solar_voltage;
    float solar_voltage_max;    // to be stored in EEPROM
    float ref_voltage;
    float dcdc_current;
    float dcdc_current_max;     // to be stored in EEPROM
    float dcdc_current_offset;
    //float input_current;
    float load_current;
    float load_current_max;     // to be stored in EEPROM
    float load_current_offset;
    float bat_current;
    float temp_int;             // °C (internal MCU temperature sensor)
    float temp_int_max;         // °C
    float temp_mosfets;         // °C
    float temp_mosfets_max;     // to be stored in EEPROM
    float temp_battery;         // °C
    bool load_enabled;
    float input_Wh_day;
    float output_Wh_day;
    float input_Wh_total;
    float output_Wh_total;
    int num_full_charges;
    int num_deep_discharges;
    int soc;
} measurement_data_t;
extern measurement_data_t meas;

typedef struct calibration_data_t {
    float dcdc_current_min;  // A     --> if lower, charger is switched off
    float dcdc_current_max;
    float load_current_max;
    bool load_overcurrent_flag;
    float solar_voltage_max; // V
    int32_t dcdc_restart_interval; // s    --> when should we retry to start charging after low solar power cut-off?
    float solar_voltage_offset_start; // V  charging switched on if Vsolar > Vbat + offset
    float solar_voltage_offset_stop;  // V  charging switched off if Vsolar < Vbat + offset
    int32_t thermistor_beta_value;  // typical value for Semitec 103AT-5 thermistor: 3435
    bool load_enabled_target;
    bool usb_enabled_target;
    bool pub_data_enabled;
} calibration_data_t;
extern calibration_data_t cal;

char manufacturer[] = "LibreSolar";
char buf[300];

static float f32;

static uint64_t ui64;
static int64_t i64;

static uint32_t ui32;
static int32_t i32;

static uint16_t ui16;
static int16_t i16;

static bool b;

static const data_object_t dataObjects[] = {
    // info
    {0x1001, TS_ACCESS_READ, TS_T_STRING, 0, (void*) manufacturer, "manufacturer"},

    // input data
    {0x3001, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL, 0, (void*) &(cal.load_enabled_target), "loadEnTarget"},
    {0x3002, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL, 0, (void*) &(cal.usb_enabled_target), "usbEnTarget"},

    // output data
    {0x4001, TS_ACCESS_READ, TS_T_INT32, 2, (void*) &(i32), "i32_output"},

    // rpc
    {0x5001, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &(cal.dcdc_current_min), "dfu"},

    // calibration data
    {0x6001, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT64,  0, (void*) &(ui64), "ui64"},
    {0x6002, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT64,   0, (void*) &(i64), "i64"},
    {0x6003, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT32,  0, (void*) &(ui32), "ui32"},
    {0x6004, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(i32), "i32"},
    {0x6005, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT16,  0, (void*) &(ui16), "ui16"},
    {0x6006, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT16,   0, (void*) &(i16), "i16"},
    {0x6007, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(f32), "f32"},
    {0x6008, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(b), "bool"},
    {0x6009, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_STRING,  sizeof(buf), (void*) buf, "strbuf"},
};


// stores object-ids of values to be stored in EEPROM
static const int eeprom_data_objects[] = {
    0x4002, 0x4004, 0x4006, 0x4008, 0x400B,     // V, I, T max
    0x400F, 0x4010, // energy throughput
    0x4011, 0x4012, // num full charge / deep-discharge
    0x3001, 0x3002  // switch targets
};

// stores object-ids of values to be stored in EEPROM
static const int pub_data_objects[] = {
    0x4001, 0x4003, 0x4005, 0x4007, 0x400A,     // V, I, T
    0x400D, 0x400E, // energy throughput
    0x4013  // SOC
};

ts_data_t data;

void test_data_init()
{
    data.objects = dataObjects;
    data.size = sizeof(dataObjects) / sizeof(data_object_t);
}

#endif
