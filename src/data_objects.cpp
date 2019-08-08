/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

#include "pcb.h"
#include "config.h"
#include "version.h"

// can be used to configure custom data objects in separate file instead (e.g. data_objects_custom.cpp)
#ifndef CUSTOM_DATA_OBJECTS_FILE

#include "thingset.h"
#include "bat_charger.h"
#include "log.h"
#include "dcdc.h"
#include "load.h"
#include "hardware.h"
#include "eeprom.h"
#include "pwm_switch.h"
#include <stdio.h>

extern LogData log_data;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;
extern Dcdc dcdc;
extern LoadOutput load;
extern DcBus hv_bus;
extern DcBus lv_bus;
extern DcBus load_bus;
extern PwmSwitch pwm_switch;

const char* const manufacturer = "Libre Solar";
const char* const device_type = DEVICE_TYPE;
const char* const hardware_version = HARDWARE_VERSION;
const char* const firmware_version = "0.1";
const char* const firmware_commit = COMMIT_HASH;
uint32_t device_id = DEVICE_ID;      // from config.h

extern uint32_t timestamp;
float mcu_temp;

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

    {0x18, TS_INFO, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32, 0, (void*) &(device_id),                                 "DeviceID"},
    {0x19, TS_INFO, TS_READ_ALL,                  TS_T_STRING, 0, (void*) manufacturer,                                 "Manufacturer"},
    {0x1A, TS_INFO, TS_READ_ALL,                  TS_T_STRING, 0, (void*) device_type,                                  "DeviceType"},
    {0x1B, TS_INFO, TS_READ_ALL,                  TS_T_STRING, 0, (void*) hardware_version,                             "HardwareVersion"},
    {0x1C, TS_INFO, TS_READ_ALL,                  TS_T_STRING, 0, (void*) firmware_version,                             "FirmwareVersion"},
    {0x1D, TS_INFO, TS_READ_ALL,                  TS_T_STRING, 0, (void*) firmware_commit,                              "FirmwareCommit"},

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    // internal unix timestamp, can be set externally. ToDO: should this use CBOR timestamp type?
    {0x01, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_UINT32,  0, (void*) &(timestamp),                                "Timestamp_s"},

    // battery settings
    {0x30, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.nominal_capacity),           "BatNom_Ah"},
    {0x31, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_recharge),           "BatRecharge_V"},
    {0x32, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_absolute_min),       "BatAbsMin_V"},
    {0x33, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.charge_current_max),         "BatChgMax_A"},
    {0x34, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_topping),            "BatTarget_V"},
    {0x35, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.current_cutoff_topping),     "BatCutoff_A"},
    {0x36, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_INT32,   0, (void*) &(bat_conf_user.time_limit_topping),         "BatCutoff_s"},
    {0x37, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    0, (void*) &(bat_conf_user.trickle_enabled),            "TrickleEn"},
    {0x38, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_trickle),            "Trickle_V"},
    {0x39, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_INT32,   0, (void*) &(bat_conf_user.time_trickle_recharge),      "TrickleRecharge_s"},
    //{0x3A, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    0, (void*) &(bat_conf_user.equalization_enabled),       "EqualEn"},
    //{0x3B, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_equalization),       "Equal_V"},
    // 0x3A-0x3E reserved for equalization charging
    {0x3F, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 3, (void*) &(bat_conf_user.temperature_compensation),   "TempFactor"},
    {0x50, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 3, (void*) &(bat_conf_user.internal_resistance),        "BatInt_Ohm"},
    {0x51, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 3, (void*) &(bat_conf_user.wire_resistance),            "BatWire_Ohm"},
    {0x52, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.charge_temp_max),            "BatChgMax_degC"},
    {0x53, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.charge_temp_min),            "BatChgMin_degC"},
    {0x54, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.discharge_temp_max),         "BatDisMax_degC"},
    {0x55, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 1, (void*) &(bat_conf_user.discharge_temp_min),         "BatDisMin_degC"},

    // load settings
    {0x40, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    0, (void*) &(load.enabled_target),                      "LoadEnDefault"},
    {0x41, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    0, (void*) &(load.usb_enabled_target),                  "UsbEnDefault"},
    {0x42, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_disconnect),    "LoadDisconnect_V"},
    {0x43, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_reconnect),     "LoadReconnect_V"},
    //{0x44, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_disconnect),    "UsbDisconnect_V"},
    //{0x45, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_FLOAT32, 2, (void*) &(bat_conf_user.voltage_load_reconnect),     "UsbReconnect_V"},

    // other configuration items
    //{0x33, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    2, (void*) &(??),   "WarningIndicator"},  // can be set externally
    //{0x33, TS_CONF, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,    2, (void*) &(??),   "ErrorIndicator"},

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    {0x60, TS_INPUT, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,   0, (void*) &(load.enabled_target),                      "LoadEn"},   // change w/o store setting in NVM
    {0x61, TS_INPUT, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,   0, (void*) &(load.usb_enabled_target),                  "UsbEn"},
#ifdef CHARGER_TYPE_PWM
    {0x62, TS_INPUT, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,   0, (void*) &(pwm_switch.enabled),                       "PwmEn"},
#else
    {0x62, TS_INPUT, TS_READ_ALL | TS_WRITE_ALL,   TS_T_BOOL,   0, (void*) &(dcdc.enabled),                             "DcdcEn"},
#endif

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    // high priority data objects (low IDs)
    {0x04, TS_OUTPUT, TS_READ_ALL, TS_T_UINT16,  0, (void*) &(load.switch_state),               "LoadState"},
    {0x05, TS_OUTPUT, TS_READ_ALL, TS_T_UINT16,  0, (void*) &(load.usb_state),                  "UsbState"},
    {0x06, TS_OUTPUT, TS_READ_ALL, TS_T_UINT16,  0, (void*) &(charger.soc),                     "SOC_%"},     // output will be uint8_t

    // battery related data objects
    {0x70, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(lv_bus.voltage),                  "Bat_V"},
    {0x71, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(hv_bus.voltage),                  "Solar_V"},
    {0x72, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(lv_bus.current),                  "Bat_A"},
    {0x73, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(load_bus.current),                "Load_A"},
    {0x74, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 1, (void*) &(charger.bat_temperature),         "Bat_degC"},
    {0x75, TS_OUTPUT, TS_READ_ALL, TS_T_BOOL,    1, (void*) &(charger.ext_temp_sensor),         "BatTempExt"},
    {0x76, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 1, (void*) &(mcu_temp),                        "MCU_degC"},
#ifdef PIN_ADC_TEMP_FETS
    {0x77, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 1, (void*) &(dcdc.temp_mosfets),               "Mosfet_degC"},
#endif
    {0x78, TS_OUTPUT, TS_READ_ALL, TS_T_UINT16,  0, (void*) &(charger.state),                   "ChgState"},
    {0x79, TS_OUTPUT, TS_READ_ALL, TS_T_UINT16,  0, (void*) &(dcdc.state),                      "DCDCState"},
    {0x7A, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(hv_bus.current),                  "Solar_A"},
    {0x7B, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(lv_bus.chg_voltage_target),       "BatTarget_V"},
    {0x7C, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(lv_bus.chg_current_max),          "BatTarget_A"},
    {0x7D, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(lv_bus.power),                    "Bat_W"},
    {0x7E, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(hv_bus.power),                    "Solar_W"},
    {0x7F, TS_OUTPUT, TS_READ_ALL, TS_T_FLOAT32, 2, (void*) &(load_bus.power),                  "Load_W"},

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    {0x08, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32,  0, (void*) &(log_data.solar_in_total_Wh),            "SolarInTotal_Wh"},
    {0x09, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32,  0, (void*) &(log_data.load_out_total_Wh),            "LoadOutTotal_Wh"},
    {0x0A, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32,  0, (void*) &(log_data.bat_chg_total_Wh),             "BatChgTotal_Wh"},
    {0x0B, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32,  0, (void*) &(log_data.bat_dis_total_Wh),             "BatDisTotal_Wh"},
    {0x0C, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  0, (void*) &(charger.num_full_charges),              "FullChgCount"},
    {0x0D, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  0, (void*) &(charger.num_deep_discharges),           "DeepDisCount"},
    {0x0E, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 0, (void*) &(charger.usable_capacity),               "BatUsable_Ah"}, // usable battery capacity
    {0x0F, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  2, (void*) &(log_data.solar_power_max_day),          "SolarMaxDay_W"},
    {0x10, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  2, (void*) &(log_data.load_power_max_day),           "LoadMaxDay_W"},

    // accumulated data
    {0xA0, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(hv_bus.dis_energy_Wh),                  "SolarInDay_Wh"},
    {0xA1, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(load_bus.chg_energy_Wh),                "LoadOutDay_Wh"},
    {0xA2, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(lv_bus.chg_energy_Wh),                  "BatChgDay_Wh"},
    {0xA3, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(lv_bus.dis_energy_Wh),                  "BatDisDay_Wh"},
    {0xA4, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 0, (void*) &(charger.discharged_Ah),                 "Dis_Ah"},    // coulomb counter
    {0xA5, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  0, (void*) &(charger.soh),                           "SOH_%"},     // output will be uint8_t
    {0xA6, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_INT32,   0, (void*) &(log_data.day_counter),                  "DayCount"},

    // min/max recordings
    {0xB1, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  2, (void*) &(log_data.solar_power_max_total),        "SolarMaxTotal_W"},
    {0xB2, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT16,  2, (void*) &(log_data.load_power_max_total),         "LoadMaxTotal_W"},
    {0xB3, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(log_data.battery_voltage_max),          "BatMaxTotal_V"},
    {0xB4, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(log_data.solar_voltage_max),            "SolarMaxTotal_V"},
    {0xB5, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(log_data.dcdc_current_max),             "DcdcMaxTotal_A"},
    {0xB6, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 2, (void*) &(log_data.load_current_max),             "LoadMaxTotal_A"},
    {0xB7, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_INT32,   1, (void*) &(log_data.bat_temp_max),                 "BatMax_degC"},
    {0xB8, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_INT32,   1, (void*) &(log_data.int_temp_max),                 "IntMax_degC"},
    {0xB9, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_INT32,   1, (void*) &(log_data.mosfet_temp_max),              "MosfetMax_degC"},
    {0xBA, TS_REC, TS_READ_ALL | TS_WRITE_MAKER, TS_T_UINT32,  1, (void*) &(log_data.error_flags),                  "ErrorFlags"},

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    {0xD0, TS_CAL, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 1, (void*) &(dcdc.ls_current_min),                   "DcdcMin_A"},
    {0xD1, TS_CAL, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 1, (void*) &(dcdc.hs_voltage_max),                   "SolarAbsMax_V"},
    {0xD2, TS_CAL, TS_READ_ALL | TS_WRITE_MAKER, TS_T_INT32,   0, (void*) &(dcdc.restart_interval),                 "DcdcRestart_s"},
    //{0xD3, TS_CAL, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_start), "SolarOffsetStart_V"},
    //{0xD4, TS_CAL, TS_READ_ALL | TS_WRITE_MAKER, TS_T_FLOAT32, 1, (void*) &(dcdc.offset_voltage_stop),  "SolarOffsetStop_V"}

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
#ifndef UNIT_TEST
    {0xE0, TS_EXEC, TS_EXEC_ALL, TS_T_BOOL, 0, (void*) &NVIC_SystemReset,     "Reset"},
#endif
    {0xE1, TS_EXEC, TS_EXEC_ALL, TS_T_BOOL, 0, (void*) &start_dfu_bootloader, "BootloaderSTM"},
    {0xE2, TS_EXEC, TS_EXEC_ALL, TS_T_BOOL, 0, (void*) &eeprom_store_data,    "SaveSettings"},
};

// stores object-ids of values to be published via Serial
const uint16_t pub_serial_ids[] = {
    0x01, // internal time stamp
    0x70, 0x71, 0x72, 0x73, 0x7A,  // voltage + current
    0x74, 0x76, 0x77,           // temperatures
    0x04, 0x78, 0x79,           // LoadState, ChgState, DCDCState
    0xA0, 0xA1, 0xA2, 0xA3,     // daily energy throughput
    0x0F, 0x10,     // SolarMaxDay_W, LoadMaxDay_W
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,     // V, I, T max
    0x06, 0xA4  // SOC, coulomb counter
};

// stores object-ids of values to be published via WiFi to Open Energy Monitor
const uint16_t pub_emoncms_ids[] = {
    0x01, // internal time stamp
    0x70, 0x71, 0x72, 0x73, 0x7A,     // voltage + current
    0x74, 0x76, /*0x77,*/           // temperatures
    0x04, 0x78, 0x79,           // LoadState, ChgState, DCDCState
    0xA0, 0xA1, 0xA2, 0xA3, // daily energy throughput
//    0x08, 0x09,     // SolarInTotal_Wh, LoadOutTotal_Wh
//    0x0A, 0x0B,     // BatChgTotal_Wh, BatDisTotal_Wh
//    0x0F, 0x10,     // SolarMaxDay_W, LoadMaxDay_W
    0x0C, 0x0D,     // FullChgCount, DeepDisCount
    0x0E, 0xA4,     // BatUsable_Ah, coulomb counter
    0x06, 0xA5, 0xA6      // SOC, SOH, DayCount
};

// stores object-ids of values to be published via CAN
const uint16_t pub_can_ids[] = {
    // 0x01, // internal time stamp
    0x73, 0x72, 0x71, 0x70, 0x7A,  // voltage + current
    0x74, 0x76, 0x77,           // temperatures
    0x04, 0x78, 0x79,           // LoadState, ChgState, DCDCState
    0xA0, 0xA1, 0xA2, 0xA3,     // daily energy throughput
    0x0F, 0x10,     // SolarMaxDay_W, LoadMaxDay_W
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,     // V, I, T max
    0x06, 0xA4,  // SOC, coulomb counter
    0x7D, 0x7E, 0x7F,
};


ts_pub_channel_t pub_channels[] = {
    { "Serial_1s",      pub_serial_ids,     sizeof(pub_serial_ids)/sizeof(uint16_t) },
    { "EmonCMS_10s",    pub_emoncms_ids,    sizeof(pub_emoncms_ids)/sizeof(uint16_t) },
    { "CAN_1s",         pub_can_ids,        sizeof(pub_can_ids)/sizeof(uint16_t) },
};

// TODO: find better solution than manual configuration of channel number
volatile const int pub_channel_serial = 0;
volatile const int pub_channel_emoncms = 1;
volatile const int PUB_CHANNEL_CAN = 2;

ThingSet ts(
    data_objects, sizeof(data_objects)/sizeof(data_object_t),
    pub_channels, sizeof(pub_channels)/sizeof(ts_pub_channel_t)
);

void data_objects_update_conf()
{
    bool changed;
    if (battery_conf_check(&bat_conf_user)) {
        printf("New config valid and activated.\n");
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
        changed = true;
    }
    else {
        printf("Check not passed, getting back old config.\n");
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
        changed = false;
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
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
    }
    else {
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }
}

#endif /* CUSTOM_DATA_OBJECTS_FILE */
