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
#include "config.h"

#include "thingset.h"
#include "battery.h"
#include "log.h"
#include "dcdc.h"
#include "load.h"
#include "hardware.h"
#include "eeprom.h"

extern log_data_t log_data;
extern battery_t bat;
extern dcdc_t dcdc;
extern load_output_t load;
extern power_port_t hs_port;
extern power_port_t ls_port;
extern battery_user_settings_t bat_user;

const char* manufacturer = "Libre Solar";
const char* deviceName = "MPPT Solar Charge Controller";
uint32_t deviceID = DEVICE_ID;      // from config.h

extern bool pub_uart_enabled;
extern bool pub_usb_enabled;

extern uint32_t timestamp;

const data_object_t data_objects[] = {

    // high priority data objects (low IDs)
    {0x01, TS_CAT_CONF,   TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT32,  0, (void*) &(timestamp),                     "Timestamp_s"},
    {0x03, TS_CAT_CONF,   TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(load.enabled_target),           "LoadEnDefault"},
    {0x04, TS_CAT_CONF,   TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(load.usb_enabled_target),       "USBEnDefault"},
    {0x05, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_INT32,   0, (void*) &(load.state),                    "LoadState"},
    {0x06, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_INT32,   0, (void*) &(load.usb_state),                "USBState"},
    {0x07, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT32,  0, (void*) &(bat.input_Wh_total),            "InputTotal_Wh"},
    {0x08, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT32,  0, (void*) &(bat.output_Wh_total),           "OutputTotal_Wh"},
    {0x09, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT16,  0, (void*) &(bat.soc),                       "SOC_%"},     // output will be uint8_t
    {0x0A, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT16,  0, (void*) &(bat.useable_capacity),          "BatUse_Ah"}, // useable battery capacity
    {0x10, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT16,  0, (void*) &(bat.num_full_charges),          "NumFullChg"},
    {0x11, TS_CAT_OUTPUT, TS_ACCESS_READ,                   TS_T_UINT16,  0, (void*) &(bat.num_deep_discharges),       "NumDeepDis"},

    // normal priority data objects (IDs > 23 = 0x17)
    {0x18, TS_CAT_INFO, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT32, 0, (void*) &(deviceID),                         "DeviceID"},
    {0x19, TS_CAT_INFO, TS_ACCESS_READ,                   TS_T_STRING, 0, (void*) &(manufacturer),                     "Manufacturer"},
    {0x1A, TS_CAT_INFO, TS_ACCESS_READ,                   TS_T_STRING, 0, (void*) &(deviceName),                       "DeviceName"},

    // config
    {0x20, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_user.nominal_capacity),         "BatNom_Ah"},
    {0x21, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.voltage_recharge),         "BatRecharge_V"},
    {0x22, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.voltage_absolute_min),     "BatAbsMin_V"},
    {0x23, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_user.charge_current_max),       "BatChgMax_A"},
    {0x24, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.voltage_max),              "BatTarget_V"},
    {0x25, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_user.current_cutoff_CV),        "BatCutoff_A"},
    {0x26, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(bat_user.time_limit_CV),            "BatCutoff_s"},
    {0x27, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(bat_user.trickle_enabled),          "TrickleEn"},
    {0x28, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 0, (void*) &(bat_user.voltage_trickle),          "Trickle_V"},
    {0x29, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(bat_user.time_trickle_recharge),    "TrickleRecharge_s"},
    {0x2A, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.voltage_load_disconnect),  "LoadDisconnect_V"},
    {0x2B, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.voltage_load_reconnect),   "LoadReconnect_V"},
    {0x2C, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_user.temperature_compensation), "TempFactor"},

    // input data
    {0x31, TS_CAT_INPUT, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,   0, (void*) &(load.enabled_target),     "LoadEnTarget"},
    {0x32, TS_CAT_INPUT, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,   0, (void*) &(load.usb_enabled_target), "USBEnTarget"},

    // output data
    {0x41, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.voltage),               "Bat_V"},
    {0x42, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.battery_voltage_max),  "BatMax_V"},
    {0x43, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(hs_port.voltage),               "Solar_V"},
    {0x44, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.solar_voltage_max),    "SolarMax_V"},
    {0x45, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.current),               "Bat_A"},
    {0x46, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.dcdc_current_max),     "BatMax_A"},
    {0x47, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(load.current),                  "Load_A"},
    {0x48, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.load_current_max),     "LoadMax_A"},
    {0x49, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(bat.temperature),               "Bat_°C"},
    {0x4A, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(dcdc.temp_mosfets),             "Internal_°C"},
    {0x4B, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(log_data.temp_mosfets_max),     "InternalMax_°C"},
    {0x4D, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(bat.input_Wh_day),              "InputDay_Wh"},
    {0x4E, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(bat.output_Wh_day),             "OutputDay_Wh"},
    {0x50, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat.state),                     "ChgState"},
    {0x51, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_INT32,   0, (void*) &(log_data.day_counter),          "DayCount"},
    {0x52, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat.soh),                       "SOH_%"},     // output will be uint8_t
    {0x53, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat.discharged_Ah),             "Dis_Ah"},    // coulomb counter
    {0x54, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat.useable_capacity),          "BatUse_Ah"}, // useable battery capacity
    // ToDo: GPS position (latitude / longitude), see here: https://github.com/allthingstalk/cbor/blob/master/CBOR-Tag103-Geographic-Coordinates.md

    // calibration data (write only after authentication)
    {0x61, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.ls_current_min),   "DcdcMin_A"},
    {0x62, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.hs_voltage_max),   "SolarAbsMax_V"},
    {0x63, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_INT32,   0, (void*) &(dcdc.restart_interval), "DcdcRestart_s"},
    //{0x6004, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_start), "SolarOffsetStart_V"},
    //{0x6005, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_stop), "SolarOffsetStop_V"}

    // rpc
    {0x71, TS_CAT_EXEC, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &start_dfu_bootloader, "bootloader"},
    {0x72, TS_CAT_EXEC, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &NVIC_SystemReset, "reset"},
};

// stores object-ids of values to be published via Serial
const uint16_t pub_serial[] = {
    0x01, // internal time stamp
    0x4001, 0x4003, 0x4005, 0x4007, 0x400A,     // V, I, T
    0x400C, // loadEn
    0x400D, 0x400E, // daily energy throughput
    0x400F, 0x4010, // total energy throughput
    0x4013, 0x4014  // SOC, chg state
};

const ts_pub_channel_t pub_channels[] = {
    { "Serial",  pub_serial,                sizeof(pub_serial)/sizeof(uint16_t) },
};

// TODO: find better solution than manual configuration of channel number
volatile const int PUB_CHANNEL_SERIAL = 0;

ThingSet ts(
    data_objects, sizeof(data_objects)/sizeof(data_object_t),
    pub_channels, sizeof(pub_channels)/sizeof(ts_pub_channel_t)
);