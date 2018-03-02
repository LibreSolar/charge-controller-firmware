/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifndef DATA_OBJECTS_H
#define DATA_OBJECTS_H

#include "stdint.h"

/*
const char* manufacturer = "Libre Solar";
const char* deviceName = "MPPT Solar Charge Controller 20A";
*/

// rename to OutputData
struct DeviceState {
    float bus_voltage;
    float input_voltage;
    float bus_current;
    float input_current;
    float load_current;
    float external_temperature;
    float internal_temperature;
    bool load_enabled;
    float input_Wh_day = 0.0;
    float output_Wh_day = 0.0;
    float input_Wh_total = 0.0;
    float output_Wh_total = 0.0;
};
extern DeviceState device;

struct CalibrationData {
    float dcdc_current_min = 0.05;  // A     --> if lower, charger is switched off
    float solar_voltage_max = 55.0; // V
    int32_t dcdc_restart_interval = 60; // s    --> when should we retry to start charging after low solar power cut-off?
    float solar_voltage_offset_start = 5.0; // V  charging switched on if Vsolar > Vbat + offset
    float solar_voltage_offset_stop = 1.0;  // V  charging switched off if Vsolar < Vbat + offset
    int32_t thermistorBetaValue = 3435;  // typical value for Semitec 103AT-5 thermistor: 3435
};
extern CalibrationData cal;

typedef struct {
    const uint16_t id;
    const uint8_t access;
    const uint8_t category;
    const uint8_t type;
    const int8_t exponent;     // only for int and uint types
    void* data;
    const char* name;
} DataObject_t;


// ThingSet CAN standard data types
#define TS_T_POS_INT8  0
#define TS_T_POS_INT16 1
#define TS_T_POS_INT32 2
#define TS_T_POS_INT64 3
#define TS_T_NEG_INT8  4
#define TS_T_NEG_INT16 5
#define TS_T_NEG_INT32 6
#define TS_T_NEG_INT64 7
#define TS_T_BYTE_STRING 8
#define TS_T_UTF8_STRING 12
#define TS_T_FLOAT32 30
#define TS_T_FLOAT64 31

#define TS_T_ARRAY 16
#define TS_T_MAP   20


// ThingSet CAN special types and simple values
#define TS_T_TIMESTAMP    33
#define TS_T_DECIMAL_FRAC 35
#define TS_T_FALSE        44
#define TS_T_TRUE         45
#define TS_T_NULL         46
#define TS_T_UNDEFINED    47

// C variable types
#define T_BOOL 0
//#define T_UINT32 1
#define T_INT32 2
#define T_FLOAT32 3
#define T_STRING 4

// ThingSet data object categories
#define TS_C_ALL 0
#define TS_C_DEVICE 1      // read-only device information (e.g. manufacturer, etc.)
#define TS_C_SETTINGS 2    // user-configurable settings (open access, maybe with user password)
#define TS_C_CAL 3         // factory-calibrated settings (access restricted)
#define TS_C_DIAGNOSIS 4   // error memory, etc (at least partly access restricted)
#define TS_C_INPUT 5       // free access
#define TS_C_OUTPUT 6      // free access

// internal access rights to data objects
#define ACCESS_READ (0x1U)
#define ACCESS_WRITE (0x1U << 1)
#define ACCESS_READ_AUTH (0x1U << 2)       // read after authentication
#define ACCESS_WRITE_AUTH (0x1U << 3)      // write after authentication

static uint16_t oid;

static const DataObject_t dataObjects[] {
    // output data
    {oid=0, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.bus_voltage), "vBat"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.input_voltage), "vSolar"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.bus_current), "iBat"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.load_current), "iLoad"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.external_temperature), "tempExt"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.internal_temperature), "tempInt"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_BOOL,    0, (void*) &(device.load_enabled), "loaEn"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.input_Wh_day), "eInputDay_Wh"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.output_Wh_day), "eOutputDay_Wh"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.input_Wh_total), "eInputTotal_Wh"},
    {++oid, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.output_Wh_total), "eOutputTotal_Wh"},

    // calibration data
    {oid=100, ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.dcdc_current_min), "iDcdcMin"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_max), "vSolarMax"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_INT32,  0, (void*) &(cal.dcdc_restart_interval), "tDcdcRestart"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_offset_start), "vSolarOffsetStart"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_offset_stop), "vSolarOffsetStop"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_INT32,  0, (void*) &(cal.thermistorBetaValue), "thermistorBetaValue"}
};

#endif
