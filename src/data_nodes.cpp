/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_nodes.h"

#ifndef UNIT_TEST
#include <zephyr.h>
#include <drivers/hwinfo.h>
#include <sys/crc.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(data_nodes, CONFIG_LOG_DEFAULT_LEVEL);
#endif

#include <stdio.h>
#include <string.h>

#include "setup.h"

// can be used to configure custom data objects in separate file instead
// (e.g. data_nodes_custom.cpp)
#ifndef CONFIG_CUSTOM_DATA_NODES_FILE

#include "thingset.h"
#include "hardware.h"
#include "dcdc.h"
#include "eeprom.h"

const char* const manufacturer = "Libre Solar";
const char* const device_type = DT_PROP(DT_PATH(pcb), type);
const char* const hardware_version = DT_PROP(DT_PATH(pcb), version_str);
const char* const firmware_version = "0.1";
const char* const firmware_commit = COMMIT_HASH;
char device_id[9];

static char auth_password[11];

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

bool pub_serial_enable = false;

#if CONFIG_THINGSET_CAN
bool pub_can_enable = false;
uint16_t ts_can_node_id = CONFIG_THINGSET_CAN_DEFAULT_NODE_ID;
#endif

/**
 * Data Objects
 *
 * IDs from 0x00 to 0x17 consume only 1 byte, so they are reserved for output data
 * objects communicated very often (to lower the data rate for LoRa and CAN)
 *
 * Normal priority data objects (consuming 2 or more bytes) start from IDs > 23 = 0x17
 */
static DataNode data_nodes[] = {

    // DEVICE INFORMATION /////////////////////////////////////////////////////
    // using IDs >= 0x18

    TS_NODE_PATH(ID_INFO, "info", 0, NULL),

    TS_NODE_STRING(0x19, "DeviceID", device_id, sizeof(device_id),
        ID_INFO, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_STRING(0x1A, "Manufacturer", manufacturer, 0,
        ID_INFO, TS_ANY_R, 0),

    TS_NODE_STRING(0x1B, "DeviceType", device_type, 0,
        ID_INFO, TS_ANY_R, 0),

    TS_NODE_STRING(0x1C, "HardwareVersion", hardware_version, 0,
        ID_INFO, TS_ANY_R, 0),

    TS_NODE_STRING(0x1D, "FirmwareVersion", firmware_version, 0,
        ID_INFO, TS_ANY_R, 0),

    TS_NODE_STRING(0x1E, "FirmwareCommit", firmware_commit, 0,
        ID_INFO, TS_ANY_R, 0),

    TS_NODE_UINT32(0x20, "Timestamp_s", &timestamp,
        ID_INFO, TS_ANY_R | TS_ANY_W, PUB_SER | PUB_NVM),

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    TS_NODE_PATH(ID_CONF, "conf", 0, &data_nodes_update_conf),

    // battery settings

    TS_NODE_FLOAT(0x31, "BatNom_Ah", &bat_conf_user.nominal_capacity, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x32, "BatRecharge_V", &bat_conf_user.voltage_recharge, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x33, "BatAbsMin_V", &bat_conf_user.voltage_absolute_min, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x34, "BatChgMax_A", &bat_conf_user.charge_current_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x35, "Topping_V", &bat_conf_user.topping_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x36, "ToppingCutoff_A", &bat_conf_user.topping_current_cutoff, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x37, "ToppingCutoff_s", &bat_conf_user.topping_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x38, "TrickleEn", &bat_conf_user.trickle_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x39, "Trickle_V", &bat_conf_user.trickle_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x3A, "TrickleRecharge_s", &bat_conf_user.trickle_recharge_time,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x3B, "EqlEn", &bat_conf_user.equalization_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x3C, "Eql_V", &bat_conf_user.equalization_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x3D, "Eql_A", &bat_conf_user.equalization_current_limit, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x3E, "EqlDuration_s", &bat_conf_user.equalization_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x3F, "EqlInterval_d", &bat_conf_user.equalization_trigger_days,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x40, "EqlDeepDisTrigger", &bat_conf_user.equalization_trigger_deep_cycles,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x41, "BatTempComp_mV-K", &bat_conf_user.temperature_compensation, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x42, "BatInt_Ohm", &bat_conf_user.internal_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x43, "BatWire_Ohm", &bat_conf_user.wire_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x44, "BatChgMax_degC", &bat_conf_user.charge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x45, "BatChgMin_degC", &bat_conf_user.charge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x46, "BatDisMax_degC", &bat_conf_user.discharge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x47, "BatDisMin_degC", &bat_conf_user.discharge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    // load settings
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_BOOL(0x50, "LoadEnDefault", &load.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x51, "LoadDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_FLOAT(0x52, "LoadReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x53, "LoadOCRecovery_s", &load.oc_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x54, "LoadUVRecovery_s", &load.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
    TS_NODE_BOOL(0x55, "UsbEnDefault", &usb_pwr.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    //TS_NODE_FLOAT(0x56, "UsbDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    //TS_NODE_FLOAT(0x57, "UsbReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_INT32(0x58, "UsbUVRecovery_s", &usb_pwr.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

#if CONFIG_THINGSET_CAN
    TS_NODE_UINT16(0x59, "CanNodeId", &ts_can_node_id,
        ID_CONF, TS_ANY_R | TS_ANY_W, PUB_NVM),
#endif

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    TS_NODE_PATH(ID_INPUT, "input", 0, NULL),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_BOOL(0x61, "LoadEn", &load.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
    TS_NODE_BOOL(0x62, "UsbEn", &usb_pwr.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    TS_NODE_BOOL(0x63, "DcdcEn", &dcdc.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    TS_NODE_BOOL(0x64, "PwmEn", &pwm_switch.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    TS_NODE_FLOAT(0x65, "GridSink_V", &hv_bus.sink_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),

    TS_NODE_FLOAT(0x66, "GridSrc_V", &hv_bus.src_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    TS_NODE_PATH(ID_OUTPUT, "output", 0, NULL),

    // battery related data objects
    TS_NODE_FLOAT(0x71, "Bat_V", &bat_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0x72, "Bat_A", &bat_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0x73, "Bat_W", &bat_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_FLOAT(0x74, "Bat_degC", &charger.bat_temperature, 1,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_BOOL(0x75, "BatTempExt", &charger.ext_temp_sensor,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_UINT16(0x76, "SOC_pct", &charger.soc, // output will be uint8_t
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_INT16(0x77, "NumBatteries", &lv_bus.series_multiplier,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_FLOAT(0x78, "Int_degC", &dev_stat.internal_temp, 1,
        ID_OUTPUT, TS_ANY_R, 0),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(adc_inputs), temp_fets))
    TS_NODE_FLOAT(0x79, "Mosfet_degC", &dcdc.temp_mosfets, 1,
        ID_OUTPUT, TS_ANY_R, 0),
#endif

    TS_NODE_FLOAT(0x7A, "ChgTarget_V", &bat_bus.sink_voltage_intercept, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_FLOAT(0x7B, "ChgTarget_A", &bat_terminal.pos_current_limit, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_UINT32(0x7C, "ChgState", &charger.state,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    TS_NODE_UINT16(0x7D, "DCDCState", &dcdc.state,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR
    TS_NODE_FLOAT(0x80, "Solar_V", &solar_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#elif CONFIG_PWM_TERMINAL_SOLAR
    TS_NODE_FLOAT(0x80, "Solar_V", &pwm_switch.ext_voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_NODE_FLOAT(0x81, "Solar_A", &solar_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_NODE_FLOAT(0x82, "Solar_W", &solar_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_FLOAT(0x89, "Load_A", &load.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0x8A, "Load_W", &load.power, 2,
        ID_OUTPUT, TS_ANY_R, 0),

    TS_NODE_INT32(0x8B, "LoadInfo", &load.info,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
    TS_NODE_INT32(0x8C, "UsbInfo", &usb_pwr.info,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    TS_NODE_FLOAT(0x90, "Grid_V", &hv_bus.voltage, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0x91, "Grid_A", &hv_terminal.current, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0x92, "Grid_W", &hv_terminal.power, 2,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
#endif

    TS_NODE_UINT32(0x9F, "ErrorFlags", &dev_stat.error_flags,
        ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    TS_NODE_PATH(ID_REC, "rec", 0, NULL),

    // accumulated data
    TS_NODE_UINT32(0x08, "SolarInTotal_Wh", &dev_stat.solar_in_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_UINT32(0x09, "LoadOutTotal_Wh", &dev_stat.load_out_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    TS_NODE_UINT32(0xC1, "GridImportTotal_Wh", &dev_stat.grid_import_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_UINT32(0xC2, "GridExportTotal_Wh", &dev_stat.grid_export_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    TS_NODE_UINT32(0x0A, "BatChgTotal_Wh", &dev_stat.bat_chg_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_UINT32(0x0B, "BatDisTotal_Wh", &dev_stat.bat_dis_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_UINT16(0x0C, "FullChgCount", &charger.num_full_charges,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_UINT16(0x0D, "DeepDisCount", &charger.num_deep_discharges,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_NVM),

    TS_NODE_FLOAT(0x0E, "BatUsable_Ah", &charger.usable_capacity, 1,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_NVM),

    TS_NODE_UINT16(0x0F, "SolarMaxDay_W", &dev_stat.solar_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_UINT16(0x10, "LoadMaxDay_W", &dev_stat.load_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    TS_NODE_FLOAT(0xA1, "SolarInDay_Wh", &solar_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_FLOAT(0xA2, "LoadOutDay_Wh", &load.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),
#endif

    TS_NODE_FLOAT(0xA3, "BatChgDay_Wh", &bat_terminal.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0xA4, "BatDisDay_Wh", &bat_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    TS_NODE_FLOAT(0xA5, "Dis_Ah", &charger.discharged_Ah, 0,   // coulomb counter
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_SER | PUB_CAN),

    TS_NODE_UINT16(0xA6, "SOH_pct", &charger.soh,    // output will be uint8_t
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

    TS_NODE_UINT32(0xA7, "DayCount", &dev_stat.day_counter,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    // min/max recordings
    TS_NODE_UINT16(0xB1, "SolarMaxTotal_W", &dev_stat.solar_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_UINT16(0xB2, "LoadMaxTotal_W", &dev_stat.load_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    TS_NODE_FLOAT(0xB3, "BatMaxTotal_V", &dev_stat.battery_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_FLOAT(0xB4, "SolarMaxTotal_V", &dev_stat.solar_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_FLOAT(0xB5, "DcdcMaxTotal_A", &dev_stat.dcdc_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    TS_NODE_FLOAT(0xB6, "LoadMaxTotal_A", &dev_stat.load_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    TS_NODE_INT16(0xB7, "BatMax_degC", &dev_stat.bat_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_INT16(0xB8, "IntMax_degC", &dev_stat.int_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_INT16(0xB9, "MosfetMax_degC", &dev_stat.mosfet_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, PUB_NVM),

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    TS_NODE_PATH(ID_CAL, "cal", 0, NULL),

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    TS_NODE_FLOAT(0xD1, "DcdcMin_W", &dcdc.output_power_min, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_FLOAT(0xD2, "SolarAbsMax_V", &dcdc.hs_voltage_max, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),

    TS_NODE_UINT32(0xD3, "DcdcRestart_s", &dcdc.restart_interval,
        ID_CAL, TS_ANY_R | TS_MKR_W, PUB_NVM),
#endif

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
    // using IDs >= 0xE0

    TS_NODE_PATH(ID_EXEC, "exec", 0, NULL),

    TS_NODE_EXEC(0xE1, "reset", &reset_device, ID_EXEC, TS_ANY_RW),
    TS_NODE_EXEC(0xE2, "bootloader-stm", &start_stm32_bootloader, ID_EXEC, TS_ANY_RW),
    TS_NODE_EXEC(0xE3, "save-settings", &eeprom_store_data, ID_EXEC, TS_ANY_RW),

    TS_NODE_EXEC(0xEE, "auth", &thingset_auth, 0, TS_ANY_RW),
    TS_NODE_STRING(0xEF, "Password", auth_password, sizeof(auth_password), 0xEE, TS_ANY_RW, 0),

    // PUBLICATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xF0

    TS_NODE_PATH(ID_PUB, "pub", 0, NULL),

    TS_NODE_PATH(0xF1, "serial", ID_PUB, NULL),
    TS_NODE_BOOL(0xF2, "Enable", &pub_serial_enable, 0xF1, TS_ANY_RW, 0),
    TS_NODE_PUBSUB(0xF3, "IDs", PUB_SER, 0xF1, TS_ANY_RW, 0),

#if CONFIG_THINGSET_CAN
    TS_NODE_PATH(0xF5, "can", ID_PUB, NULL),
    TS_NODE_BOOL(0xF6, "Enable", &pub_can_enable, 0xF5, TS_ANY_RW, 0),
    TS_NODE_PUBSUB(0xF7, "IDs", PUB_CAN, 0xF5, TS_ANY_RW, 0),
#endif
};

ThingSet ts(data_nodes, sizeof(data_nodes)/sizeof(DataNode));

void data_nodes_update_conf()
{
    bool changed;
    if (battery_conf_check(&bat_conf_user)) {
        LOG_INF("New config valid and activated.");
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
        load.set_voltage_limits(bat_conf.voltage_load_disconnect, bat_conf.voltage_load_reconnect,
            bat_conf.voltage_absolute_max);
#endif
        changed = true;
    }
    else {
        LOG_ERR("Requested config change not valid and rejected.");
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
        changed = false;
    }

    // TODO: check also for changes in Load/USB EnDefault
    changed = true; // temporary hack

    if (changed) {
        eeprom_store_data();
    }
}

void data_nodes_init()
{
#ifndef UNIT_TEST
    uint8_t buf[12];
    hwinfo_get_device_id(buf, sizeof(buf));

    uint64_t id64 = crc32_ieee(buf, sizeof(buf));
    id64 += ((uint64_t)CONFIG_LIBRE_SOLAR_TYPE_ID) << 32;

    uint64_to_base32(id64, device_id, sizeof(device_id), alphabet_crockford);
#endif

    eeprom_restore_data();
    if (battery_conf_check(&bat_conf_user)) {
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
    }
    else {
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }
}

void thingset_auth()
{
    static const char pass_exp[] = CONFIG_THINGSET_EXPERT_PASSWORD;
    static const char pass_mkr[] = CONFIG_THINGSET_MAKER_PASSWORD;

    if (strlen(pass_exp) == strlen(auth_password) &&
        strncmp(auth_password, pass_exp, strlen(pass_exp)) == 0)
    {
        LOG_INF("Authenticated as expert user.");
        ts.set_authentication(TS_EXP_MASK | TS_USR_MASK);
    }
    else if (strlen(pass_mkr) == strlen(auth_password) &&
        strncmp(auth_password, pass_mkr, strlen(pass_mkr)) == 0)
    {
        LOG_INF("Authenticated as maker.");
        ts.set_authentication(TS_MKR_MASK | TS_USR_MASK);
    }
    else {
        LOG_INF("Reset authentication.");
        ts.set_authentication(TS_USR_MASK);
    }
}

void uint64_to_base32(uint64_t in, char *out, size_t size, const char *alphabet)
{
    // 13 is the maximum number of characters needed to encode 64-bit variable to base32
    int len = (size > 13) ? 13 : size;

    // find out actual length of output string
    for (int i = 0; i < len; i++) {
        if ((in >> (i * 5)) == 0) {
            len = i;
            break;
        }
    }

    for (int i = 0; i < len; i++) {
        out[len-i-1] = alphabet[(in >> (i * 5)) % 32];
    }
    out[len] = '\0';
}


#endif /* CUSTOM_DATA_NODES_FILE */
