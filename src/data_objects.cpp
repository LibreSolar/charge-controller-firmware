/* LibreSolar Battery Management System firmware
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

#include "mbed.h"
#include "config.h"

#include "data_objects.h"


static uint16_t oid;

const DataObject_t dataObjects[] {
    // output data
    {oid=0x4001, ACCESS_READ, TS_C_OUTPUT, T_FLOAT32, 0, (void*) &(device.bus_voltage), "vBat"},
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
    {oid=0x6001, ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.dcdc_current_min), "iDcdcMin"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_max), "vSolarMax"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_INT32,  0, (void*) &(cal.dcdc_restart_interval), "tDcdcRestart"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_offset_start), "vSolarOffsetStart"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_FLOAT32, 0, (void*) &(cal.solar_voltage_offset_stop), "vSolarOffsetStop"},
    {++oid,   ACCESS_READ | ACCESS_WRITE, TS_C_CAL, T_INT32,  0, (void*) &(cal.thermistor_beta_value), "thermistorBetaValue"}
};

const size_t dataObjectsCount = sizeof(dataObjects)/sizeof(DataObject_t);

