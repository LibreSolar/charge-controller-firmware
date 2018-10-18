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
#include "pcb.h"

#include "data_objects.h"

const char* manufacturer = "Libre Solar";
const char* deviceName = "MPPT Solar Charge Controller 20A";
const char* deviceID = "LSMPPT-00001";

const data_object_t dataObjects[] = {
    // info
    {0x1003, TS_ACCESS_READ, TS_T_STRING, 0, (void*) &(deviceID), "deviceID"},

    // input data
    {0x3001, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL, 0, (void*) &(load.enabled_target), "loadEnTarget"},
    {0x3002, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL, 0, (void*) &(load.usb_enabled_target), "usbEnTarget"},

    // output data
    {0x4001, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.voltage), "vBat"},
    {0x4002, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.battery_voltage_max), "vBatMax"},
    {0x4003, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(hs_port.voltage), "vSolar"},
    {0x4004, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.solar_voltage_max), "vSolarMax"},
    {0x4005, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.current), "iBat"},
    {0x4006, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.dcdc_current_max), "iBatMax"},
    {0x4007, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(load.current), "iLoad"},
    {0x4008, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.load_current_max), "iLoadMax"},
    {0x4009, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(bat.temperature), "tempBat"},
    {0x400A, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(dcdc.temp_mosfets), "tempInt"},  // TODO
    {0x400B, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(log_data.temp_mosfets_max), "tempIntMax"}, // TODO
    {0x400C, TS_ACCESS_READ, TS_T_BOOL,    0, (void*) &(load.enabled), "loadEn"},
    {0x400D, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(bat.input_Wh_day), "eInputDay_Wh"},
    {0x400E, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(bat.output_Wh_day), "eOutputDay_Wh"},
    {0x400F, TS_ACCESS_READ, TS_T_FLOAT32, 0, (void*) &(bat.input_Wh_total), "eInputTotal_Wh"},
    {0x4010, TS_ACCESS_READ, TS_T_FLOAT32, 0, (void*) &(bat.output_Wh_total), "eOutputTotal_Wh"},
    {0x4011, TS_ACCESS_READ, TS_T_INT32,   0, (void*) &(bat.num_full_charges), "nFullChg"},
    {0x4012, TS_ACCESS_READ, TS_T_INT32,   0, (void*) &(bat.num_deep_discharges), "nDeepDis"},
    {0x4013, TS_ACCESS_READ, TS_T_INT32,   1, (void*) &(bat.soc), "SOC"},
/*
    // rpc
    {0x5001, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &(cal.dcdc_current_min), "dfu"},
*/
    // calibration data
    {0x6001, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(dcdc.ls_current_min), "iDcdcMin"},
    {0x6002, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(dcdc.hs_voltage_max), "vSolarAbsMax"},
    {0x6003, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(dcdc.restart_interval), "tDcdcRestart"},
    //{0x6004, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_start), "vSolarOffsetStart"},
    //{0x6005, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_stop), "vSolarOffsetStop"}
};

const size_t dataObjectsCount = sizeof(dataObjects)/sizeof(data_object_t);
