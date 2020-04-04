/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST
#include <zephyr.h>
#endif

#include <stdio.h>

#include "setup.h"

#include "board.h"
#include "version.h"

// can be used to configure custom data objects in separate file instead
// (e.g. data_objects_custom.cpp)
#ifndef CUSTOM_DATA_OBJECTS_FILE

#include "thingset.h"
#include "hardware.h"
#include "dcdc.h"
#include "eeprom.h"

const char* const manufacturer = "Libre Solar";
const char* const device_type = DEVICE_TYPE;
const char* const hardware_version = HARDWARE_VERSION;
const char* const firmware_version = "0.1";
const char* const firmware_commit = COMMIT_HASH;
uint32_t device_id = CONFIG_DEVICE_ID;

#if CONFIG_LV_TERMINAL_BATTERY
#define bat_bus lv_bus
#elif CONFIG_HV_TERMINAL_BATTERY
#define bat_bus hv_bus
#endif

#if CONFIG_HV_TERMINAL_SOLAR
#define solar_bus hv_bus
#elif CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
#define solar_bus lv_bus
#endif

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

    TS_DATA_OBJ_UINT32(0x18, "DeviceID", &(device_id),
        TS_INFO, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_STRING(0x19, "Manufacturer", manufacturer, 0,
        TS_INFO, TS_READ_ALL),

    TS_DATA_OBJ_STRING(0x1A, "DeviceType", device_type, 0,
        TS_INFO, TS_READ_ALL),

    TS_DATA_OBJ_STRING(0x1B, "HardwareVersion", hardware_version, 0,
        TS_INFO, TS_READ_ALL),

    TS_DATA_OBJ_STRING(0x1C, "FirmwareVersion", firmware_version, 0,
        TS_INFO, TS_READ_ALL),

    TS_DATA_OBJ_STRING(0x1D, "FirmwareCommit", firmware_commit, 0,
        TS_INFO, TS_READ_ALL),

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    // internal unix timestamp, can be set externally
    TS_DATA_OBJ_UINT32(0x01, "Timestamp_s", &timestamp,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    // battery settings

    TS_DATA_OBJ_FLOAT(0x30, "BatNom_Ah", &bat_conf_user.nominal_capacity, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x31, "BatRecharge_V", &bat_conf_user.voltage_recharge, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x32, "BatAbsMin_V", &bat_conf_user.voltage_absolute_min, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x33, "BatChgMax_A", &bat_conf_user.charge_current_max, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x34, "BatTarget_V", &bat_conf_user.topping_voltage, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x35, "BatCutoff_A", &bat_conf_user.topping_current_cutoff, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x36, "BatCutoff_s", &bat_conf_user.topping_duration,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_BOOL(0x37, "TrickleEn", &bat_conf_user.trickle_enabled,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x38, "Trickle_V", &bat_conf_user.trickle_voltage, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x39, "TrickleRecharge_s", &bat_conf_user.trickle_recharge_time,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x3F, "TempFactor", &bat_conf_user.temperature_compensation, 3,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x50, "BatInt_Ohm", &bat_conf_user.internal_resistance, 3,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x51, "BatWire_Ohm", &bat_conf_user.wire_resistance, 3,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x52, "BatChgMax_degC", &bat_conf_user.charge_temp_max, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x53, "BatChgMin_degC", &bat_conf_user.charge_temp_min, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x54, "BatDisMax_degC", &bat_conf_user.discharge_temp_max, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x55, "BatDisMin_degC", &bat_conf_user.discharge_temp_min, 1,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_BOOL(0x58, "EqEn", &bat_conf_user.equalization_enabled,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x59, "Eq_V", &bat_conf_user.equalization_voltage, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x5A, "Eq_A", &bat_conf_user.equalization_current_limit, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x5B, "EqDuration_s", &bat_conf_user.equalization_duration,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x5C, "EqInterval_d", &bat_conf_user.equalization_trigger_days,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x5D, "EqDeepDisTrigger", &bat_conf_user.equalization_trigger_deep_cycles,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    // load settings
    TS_DATA_OBJ_BOOL(0x40, "LoadEnDefault", &load.enable,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

#if CONFIG_HAS_USB_PWR_OUTPUT
    TS_DATA_OBJ_BOOL(0x41, "UsbEnDefault", &usb_pwr.enable,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),
#endif

    TS_DATA_OBJ_FLOAT(0x42, "LoadDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x43, "LoadReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    //TS_DATA_OBJ_FLOAT(0x44, "UsbDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
    //    TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    //TS_DATA_OBJ_FLOAT(0x45, "UsbReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
    //    TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x46, "LoadOCRecovery_s", &load.oc_recovery_delay,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_INT32(0x47, "LoadUVRecovery_s", &load.lvd_recovery_delay,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),

#if CONFIG_HAS_USB_PWR_OUTPUT
    TS_DATA_OBJ_INT32(0x48, "UsbUVRecovery_s", &usb_pwr.lvd_recovery_delay,
        TS_CONF, TS_READ_ALL | TS_WRITE_ALL),
#endif

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    TS_DATA_OBJ_BOOL(0x60, "LoadEn", &load.enable,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),

#if CONFIG_HAS_USB_PWR_OUTPUT
    TS_DATA_OBJ_BOOL(0x61, "UsbEn", &usb_pwr.enable,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),
#endif

#if DT_COMPAT_DCDC
    TS_DATA_OBJ_BOOL(0x62, "DcdcEn", &dcdc.enable,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),
#endif

#if DT_COMPAT_PWM_SWITCH
    TS_DATA_OBJ_BOOL(0x63, "PwmEn", &pwm_switch.enable,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    TS_DATA_OBJ_FLOAT(0x64, "GridSink_V", &hv_bus.sink_voltage_bound, 2,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),

    TS_DATA_OBJ_FLOAT(0x66, "GridSrc_V", &hv_bus.src_voltage_bound, 2,
        TS_INPUT, TS_READ_ALL | TS_WRITE_ALL),
#endif

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    // high priority data objects (low IDs)
    TS_DATA_OBJ_INT32(0x04, "LoadInfo", &load.info,
        TS_OUTPUT, TS_READ_ALL),

#if CONFIG_HAS_USB_PWR_OUTPUT
    TS_DATA_OBJ_INT32(0x05, "UsbInfo", &usb_pwr.info,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_UINT16(0x06, "SOC_%", &charger.soc, // output will be uint8_t
        TS_OUTPUT, TS_READ_ALL),

    // battery related data objects
    TS_DATA_OBJ_FLOAT(0x70, "Bat_V", &bat_bus.voltage, 2,
        TS_OUTPUT, TS_READ_ALL),

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_DATA_OBJ_FLOAT(0x71, "Solar_V", &solar_bus.voltage, 2,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_FLOAT(0x72, "Bat_A", &bat_terminal.current, 2,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_FLOAT(0x73, "Load_A", &load.current, 2,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_FLOAT(0x74, "Bat_degC", &charger.bat_temperature, 1,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_BOOL(0x75, "BatTempExt", &charger.ext_temp_sensor,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_FLOAT(0x76, "Int_degC", &dev_stat.internal_temp, 1,
        TS_OUTPUT, TS_READ_ALL),

#if defined(PIN_ADC_TEMP_FETS) && DT_COMPAT_DCDC
    TS_DATA_OBJ_FLOAT(0x77, "Mosfet_degC", &dcdc.temp_mosfets, 1,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_UINT32(0x78, "ChgState", &charger.state,
        TS_OUTPUT, TS_READ_ALL),

#if DT_COMPAT_DCDC
    TS_DATA_OBJ_UINT16(0x79, "DCDCState", &dcdc.state,
        TS_OUTPUT, TS_READ_ALL),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_DATA_OBJ_FLOAT(0x7A, "Solar_A", &solar_terminal.current, 2,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_FLOAT(0x7B, "BatTarget_V", &bat_bus.sink_voltage_bound, 2,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_FLOAT(0x7C, "BatTarget_A", &bat_terminal.pos_current_limit, 2,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_FLOAT(0x7D, "Bat_W", &bat_terminal.power, 2,
        TS_OUTPUT, TS_READ_ALL),

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_DATA_OBJ_FLOAT(0x7E, "Solar_W", &solar_terminal.power, 2,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_FLOAT(0x7F, "Load_W", &load.power, 2,
        TS_OUTPUT, TS_READ_ALL),

#if CONFIG_HV_TERMINAL_NANOGRID
    TS_DATA_OBJ_FLOAT(0x80, "Grid_V", &hv_bus.voltage, 2,
        TS_OUTPUT, TS_READ_ALL),
#endif

    TS_DATA_OBJ_INT16(0x8F, "NumBatteries", &lv_bus.series_multiplier,
        TS_OUTPUT, TS_READ_ALL),

    TS_DATA_OBJ_UINT32(0x90, "ErrorFlags", &dev_stat.error_flags,
        TS_OUTPUT, TS_READ_ALL),

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    // accumulated data
    TS_DATA_OBJ_UINT32(0x08, "SolarInTotal_Wh", &dev_stat.solar_in_total_Wh,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT32(0x09, "LoadOutTotal_Wh", &dev_stat.load_out_total_Wh,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT32(0x0A, "BatChgTotal_Wh", &dev_stat.bat_chg_total_Wh,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT32(0x0B, "BatDisTotal_Wh", &dev_stat.bat_dis_total_Wh,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0x0C, "FullChgCount", &charger.num_full_charges,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0x0D, "DeepDisCount", &charger.num_deep_discharges,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0x0E, "BatUsable_Ah", &charger.usable_capacity, 1,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0x0F, "SolarMaxDay_W", &dev_stat.solar_power_max_day,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0x10, "LoadMaxDay_W", &dev_stat.load_power_max_day,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_DATA_OBJ_FLOAT(0xA0, "SolarInDay_Wh", &solar_terminal.neg_energy_Wh, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),
#endif

    TS_DATA_OBJ_FLOAT(0xA1, "LoadOutDay_Wh", &load.pos_energy_Wh, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xA2, "BatChgDay_Wh", &bat_terminal.pos_energy_Wh, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xA3, "BatDisDay_Wh", &bat_terminal.neg_energy_Wh, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xA4, "Dis_Ah", &charger.discharged_Ah, 0,   // coulomb counter
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0xA5, "SOH_%", &charger.soh,    // output will be uint8_t
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT32(0xA6, "DayCount", &dev_stat.day_counter,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    // min/max recordings
    TS_DATA_OBJ_UINT16(0xB1, "SolarMaxTotal_W", &dev_stat.solar_power_max_total,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT16(0xB2, "LoadMaxTotal_W", &dev_stat.load_power_max_total,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xB3, "BatMaxTotal_V", &dev_stat.battery_voltage_max, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xB4, "SolarMaxTotal_V", &dev_stat.solar_voltage_max, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xB5, "DcdcMaxTotal_A", &dev_stat.dcdc_current_max, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xB6, "LoadMaxTotal_A", &dev_stat.load_current_max, 2,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_INT16(0xB7, "BatMax_degC", &dev_stat.bat_temp_max,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_INT16(0xB8, "IntMax_degC", &dev_stat.int_temp_max,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_INT16(0xB9, "MosfetMax_degC", &dev_stat.mosfet_temp_max,
        TS_REC, TS_READ_ALL | TS_WRITE_MAKER),

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

#if DT_COMPAT_DCDC
    TS_DATA_OBJ_FLOAT(0xD0, "DcdcMin_W", &dcdc.output_power_min, 1,
        TS_CAL, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_FLOAT(0xD1, "SolarAbsMax_V", &dcdc.hs_voltage_max, 1,
        TS_CAL, TS_READ_ALL | TS_WRITE_MAKER),

    TS_DATA_OBJ_UINT32(0xD2, "DcdcRestart_s", &dcdc.restart_interval,
        TS_CAL, TS_READ_ALL | TS_WRITE_MAKER),
#endif

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
    // using IDs >= 0xE0

    TS_DATA_OBJ_EXEC(0xE0, "Reset", &reset_device, TS_EXEC_ALL),
    TS_DATA_OBJ_EXEC(0xE1, "BootloaderSTM", &start_stm32_bootloader, TS_EXEC_ALL),
    TS_DATA_OBJ_EXEC(0xE2, "SaveSettings", &eeprom_store_data, TS_EXEC_ALL),
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
    /*0x74,*/ 0x76, /*0x77,*/         // temperatures
    0x04, 0x78, 0x79,           // LoadState, ChgState, DCDCState
    0xA0, 0xA1, 0xA2, 0xA3, // daily energy throughput
//    0x08, 0x09,     // SolarInTotal_Wh, LoadOutTotal_Wh
//    0x0A, 0x0B,     // BatChgTotal_Wh, BatDisTotal_Wh
//    0x0F, 0x10,     // SolarMaxDay_W, LoadMaxDay_W
    0x0C, 0x0D,     // FullChgCount, DeepDisCount
    0x0E, 0xA4,     // BatUsable_Ah, coulomb counter
    0x06, 0xA5, 0xA6,      // SOC, SOH, DayCount
    0x90            // error flags
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
        load.set_voltage_limits(bat_conf.voltage_load_disconnect, bat_conf.voltage_load_reconnect,
            bat_conf.voltage_absolute_max);
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
