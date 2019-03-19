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
extern battery_state_t bat_state;
extern battery_conf_t bat_conf;
extern battery_conf_t bat_conf_user;
extern dcdc_t dcdc;
extern load_output_t load;
extern power_port_t hs_port;
extern power_port_t ls_port;

const char* manufacturer = "Libre Solar";
const char* deviceName = "MPPT Solar Charge Controller";
uint32_t deviceID = DEVICE_ID;      // from config.h

extern bool pub_uart_enabled;
extern bool pub_usb_enabled;

extern uint32_t timestamp;
float latitude;                 // can be read out via GSM module for some network operators
float longitude;

/** Data Objects
 *
 * IDs from 0x00 to 0x17 consume only 1 byte, so they are reserved for output data
 * objects communicated very often (to lower the data rate for LoRa and CAN)
 *
 * Normal priority data objects (consuming 2 or more bytes) start from IDs > 23 = 0x17
 */

const data_object_t data_objects[] = {

    // DEVICE INFORMATION /////////////////////////////////////////////////////
    // using IDs >= 0x18

    {0x18, TS_CAT_INFO, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT32, 0, (void*) &(deviceID),                       "DeviceID"},       // ToDo: TS_ACCESS_WRITE_AUTH only
    {0x19, TS_CAT_INFO, TS_ACCESS_READ,                   TS_T_STRING, 0, (void*) manufacturer,                      "Manufacturer"},
    {0x1A, TS_CAT_INFO, TS_ACCESS_READ,                   TS_T_STRING, 0, (void*) deviceName,                        "DeviceName"},
    //{0x1B, TS_CAT_INFO, TS_ACCESS_READ,                   TS_T_STRING, 0, (void*) &(?),                     "FirmwareVersion"},    // ToDo

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    // internal unix timestamp, can be set externally. ToDO: should this use CBOR timestamp type?
    {0x01, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_UINT32,  0, (void*) &(timestamp),                                "Timestamp_s"},

    // battery settings
    {0x30, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_conf_user.nominal_capacity),           "BatNom_Ah"},
    {0x31, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_recharge),           "BatRecharge_V"},
    {0x32, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_absolute_min),       "BatAbsMin_V"},
    {0x33, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_conf_user.charge_current_max),         "BatChgMax_A"},
    {0x34, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_max),                "BatTarget_V"},
    {0x35, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 1, (void*) &(bat_conf_user.current_cutoff_CV),          "BatCutoff_A"},
    {0x36, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(bat_conf_user.time_limit_CV),              "BatCutoff_s"},
    {0x37, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(bat_conf_user.trickle_enabled),            "TrickleEn"},
    {0x37, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 0, (void*) &(bat_conf_user.voltage_trickle),            "Trickle_V"},
    {0x38, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_INT32,   0, (void*) &(bat_conf_user.time_trickle_recharge),      "TrickleRecharge_s"},
    // 0x39-0x3E reserved for equalization charging
    {0x3F, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.temperature_compensation),   "TempFactor"},

    // load settings
    {0x40, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(load.enabled_target),                      "LoadEnDefault"},
    {0x41, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    0, (void*) &(load.usb_enabled_target),                  "USBEnDefault"},
    {0x42, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_disconnect),    "LoadDisconnect_V"},
    {0x43, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_reconnect),     "LoadReconnect_V"},
    //{0x44, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_disconnect),    "USBDisconnect_V"},
    //{0x45, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_reconnect),     "USBReconnect_V"},

    // other configuration items
    //{0x33, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    2, (void*) &(??),   "WarningIndicator"},  // can be set externally
    //{0x33, TS_CAT_CONF, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,    2, (void*) &(??),   "ErrorIndicator"},

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    {0x60, TS_CAT_INPUT, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,   0, (void*) &(load.enabled_target),               "LoadEn"},   // change w/o store setting in NVM
    {0x61, TS_CAT_INPUT, TS_ACCESS_READ | TS_ACCESS_WRITE, TS_T_BOOL,   0, (void*) &(load.usb_enabled_target),           "USBEn"},

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    // high priority data objects (low IDs)
    {0x04, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(load.switch_state),             "LoadState"},
    {0x05, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(load.usb_state),                "USBState"},
    {0x06, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.soc),                 "SOC_%"},     // output will be uint8_t

    // battery related data objects
    {0x70, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.voltage),               "Bat_V"},
    {0x71, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(hs_port.voltage),               "Solar_V"},
    {0x72, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(ls_port.current),               "Bat_A"},
    {0x73, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(load.current),                  "Load_A"},
    {0x74, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(bat_state.temperature),         "Bat_degC"},
    {0x75, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 1, (void*) &(dcdc.temp_mosfets),             "Internal_degC"},
    {0x76, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.chg_state),           "ChgState"},

    // others
    {0x90, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 0, (void*) &(latitude),                      "Latitude"},
    {0x91, TS_CAT_OUTPUT, TS_ACCESS_READ, TS_T_FLOAT32, 0, (void*) &(longitude),                     "Longitude"},

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    {0x08, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT32,  0, (void*) &(log_data.solar_in_total_Wh),           "SolarInTotal_Wh"},
    {0x09, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT32,  0, (void*) &(log_data.load_out_total_Wh),           "LoadOutTotal_Wh"},
    {0x0A, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT32,  0, (void*) &(bat_state.chg_total_Wh),               "BatChgTotal_Wh"},
    {0x0B, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT32,  0, (void*) &(bat_state.dis_total_Wh),               "BatDisTotal_Wh"},
    {0x0C, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.num_full_charges),           "FullChgCount"},
    {0x0D, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.num_deep_discharges),        "DeepDisCount"},
    {0x0E, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.usable_capacity),            "BatUsable_Ah"}, // usable battery capacity
    {0x0F, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  2, (void*) &(log_data.solar_power_max_day),         "SolarMaxDay_W"},
    {0x10, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  2, (void*) &(log_data.load_power_max_day),          "LoadMaxDay_W"},

    // accumulated data
    {0xA0, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.solar_in_day_Wh),             "SolarInDay_Wh"},
    {0xA1, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.load_out_day_Wh),             "LoadOutDay_Wh"},
    {0xA2, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(bat_state.chg_day_Wh),                 "BatChgDay_Wh"},
    {0xA3, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(bat_state.dis_day_Wh),                 "BatDisDay_Wh"},
    {0xA4, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 0, (void*) &(bat_state.discharged_Ah),              "Dis_Ah"},    // coulomb counter
    {0xA5, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  0, (void*) &(bat_state.soh),                        "SOH_%"},     // output will be uint8_t
    {0xA6, TS_CAT_REC, TS_ACCESS_READ, TS_T_INT32,   0, (void*) &(log_data.day_counter),                 "DayCount"},

    // min/max recordings
    {0xB1, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  2, (void*) &(log_data.solar_power_max_total),       "SolarMaxTotal_W"},
    {0xB2, TS_CAT_REC, TS_ACCESS_READ, TS_T_UINT16,  2, (void*) &(log_data.load_power_max_total),        "LoadMaxTotal_W"},
    {0xB3, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.battery_voltage_max),         "BatMaxTotal_V"},
    {0xB4, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.solar_voltage_max),           "SolarMaxTotal_V"},
    {0xB5, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.dcdc_current_max),            "DCDCMaxTotal_A"},
    {0xB6, TS_CAT_REC, TS_ACCESS_READ, TS_T_FLOAT32, 2, (void*) &(log_data.load_current_max),            "LoadMaxTotal_A"},
    {0xB7, TS_CAT_REC, TS_ACCESS_READ, TS_T_INT32, 1, (void*) &(log_data.bat_temp_max),                  "BatMax_degC"},
    {0xB8, TS_CAT_REC, TS_ACCESS_READ, TS_T_INT32, 1, (void*) &(log_data.int_temp_max),                  "IntMax_degC"},
    {0xB9, TS_CAT_REC, TS_ACCESS_READ, TS_T_INT32, 1, (void*) &(log_data.mosfet_temp_max),               "MosfetMax_degC"},

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    {0xD0, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.ls_current_min),   "DcdcMin_A"},
    {0xD1, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.hs_voltage_max),   "SolarAbsMax_V"},
    {0xD2, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_INT32,   0, (void*) &(dcdc.restart_interval), "DcdcRestart_s"},
    //{0xD3, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_start), "SolarOffsetStart_V"},
    //{0xD4, TS_CAT_CAL, TS_ACCESS_READ | TS_ACCESS_WRITE_AUTH, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_stop), "SolarOffsetStop_V"}

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
    {0xE0, TS_CAT_EXEC, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &NVIC_SystemReset,     "Reset"},
    {0xE1, TS_CAT_EXEC, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &start_dfu_bootloader, "Bootloader"},
    {0xE2, TS_CAT_EXEC, TS_ACCESS_EXEC, TS_T_BOOL, 0, (void*) &eeprom_store_data,    "SaveSettings"},
};

// stores object-ids of values to be published via Serial
const uint16_t pub_serial[] = {
    0x01, // internal time stamp
    0x70, 0x71, 0x72, 0x73, 0x74,     // V, I, T
    0x04, 0x76, // LoadState, ChgState
    0xA0, 0xA1, 0xA2, 0xA3, // daily energy throughput
    0x06, 0xA4  // SOC, coulomb counter
};

const ts_pub_channel_t pub_channels[] = {
    { "Serial",  pub_serial,            sizeof(pub_serial)/sizeof(uint16_t) },
};

// TODO: find better solution than manual configuration of channel number
volatile const int PUB_CHANNEL_SERIAL = 0;

ThingSet ts(
    data_objects, sizeof(data_objects)/sizeof(data_object_t),
    pub_channels, sizeof(pub_channels)/sizeof(ts_pub_channel_t)
);

void data_objects_update_conf()
{
    bool changed = false;
    if (battery_conf_check(&bat_conf_user)) {
        printf("New config valid and activated.\n");
        battery_conf_overwrite(&bat_conf_user, &bat_conf);
        changed = true;
    }
    else {
        printf("Check not passed, getting back old config.\n");
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }

    // TODO: check also for changes in Load/USB EnDefault
    changed = true; // temporary hack

    if (changed)
        eeprom_store_data();
}

void data_objects_read_eeprom()
{
    eeprom_restore_data();
    if (battery_conf_check(&bat_conf_user)) {
        battery_conf_overwrite(&bat_conf_user, &bat_conf);
    }
    else {
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }
}
