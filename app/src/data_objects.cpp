/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_objects.h"

#include <soc.h>
#include <zephyr.h>

#include <drivers/hwinfo.h>
#include <sys/crc.h>

#ifdef CONFIG_SOC_FAMILY_STM32
#include <stm32_ll_utils.h>
#endif

#include <stdio.h>
#include <string.h>

#include "data_storage.h"
#include "dcdc.h"
#include "hardware.h"
#include "helper.h"
#include "setup.h"
#include "thingset.h"

LOG_MODULE_REGISTER(data_objects, CONFIG_DATA_OBJECTS_LOG_LEVEL);

// can be used to configure custom data objects in separate file instead
// (e.g. data_objects_custom.cpp)
#ifndef CONFIG_CUSTOM_DATA_OBJECTS_FILE

const char manufacturer[] = "Libre Solar";
const char metadata_url[] =
    "https://files.libre.solar/tsm/cc-" STRINGIFY(DATA_OBJECTS_VERSION) ".json";
const char device_type[] = DT_PROP(DT_PATH(pcb), type);
const char hardware_version[] = DT_PROP(DT_PATH(pcb), version_str);
const char firmware_version[] = FIRMWARE_VERSION_ID;
char device_id[9];

#ifdef CONFIG_SOC_FAMILY_STM32
uint32_t flash_size = LL_GetFlashSize();
uint32_t flash_page_size = FLASH_PAGE_SIZE;
#else
uint32_t flash_size = 128;
uint32_t flash_page_size = 0x800;
#endif

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

bool pub_serial_enable = IS_ENABLED(CONFIG_THINGSET_SERIAL_PUB_DEFAULT);

#if CONFIG_THINGSET_CAN
bool pub_can_enable = IS_ENABLED(CONFIG_THINGSET_CAN_PUB_DEFAULT);
uint16_t can_node_addr = CONFIG_THINGSET_CAN_DEFAULT_NODE_ID;
#endif

/**
 * Thing Set Data Objects (see thingset.io for specification)
 */
/* clang-format off */
static ThingSetDataObject data_objects[] = {

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /*{
        "title": {
            "en": "ThingSet Node ID",
            "de": "ThingSet Knoten-ID"
        }
    }*/
    TS_ITEM_STRING(0x1D, "cNodeID", device_id, sizeof(device_id),
        ID_ROOT, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "ThingSet Metadata URL",
            "de": "ThingSet Metadata URL"
        }
    }*/
    TS_ITEM_STRING(0x18, "cMetadataURL", metadata_url, sizeof(metadata_url),
        ID_ROOT, TS_ANY_R, 0),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TS_GROUP(ID_DEVICE, "Device", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Manufacturer",
            "de": "Hersteller"
        }
    }*/
    TS_ITEM_STRING(0x20, "cManufacturer", manufacturer, 0,
        ID_DEVICE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Device Type",
            "de": "Gerätetyp"
        }
    }*/
    TS_ITEM_STRING(0x21, "cType", device_type, 0,
        ID_DEVICE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Hardware Version",
            "de": "Hardware-Version"
        }
    }*/
    TS_ITEM_STRING(0x22, "cHardwareVersion", hardware_version, 0,
        ID_DEVICE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Firmware Version",
            "de": "Firmware-Version"
        }
    }*/
    TS_ITEM_STRING(0x23, "cFirmwareVersion", firmware_version, 0,
        ID_DEVICE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Time since last reset",
            "de": "Zeit seit Systemstart"
        }
    }*/
    TS_ITEM_UINT32(0x30, "rUptime_s", &timestamp,
        ID_DEVICE, TS_ANY_R, SUBSET_SER),

    /*{
        "title": {
            "en": "Error Flags",
            "de": "Fehlercode"
        }
    }*/
    TS_ITEM_UINT32(0x5F, "rErrorFlags", &dev_stat.error_flags,
        ID_DEVICE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Internal Temperature",
            "de": "Interne Temperatur"
        }
    }*/
    TS_ITEM_FLOAT(0x36, "rInt_degC", &dev_stat.internal_temp, 1,
        ID_DEVICE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Peak Internal Temperature (all-time)",
            "de": "Interne Maximaltemperatur (gesamt)"
        }
    }*/
    TS_ITEM_INT16(0x79, "pIntMax_degC", &dev_stat.int_temp_max,
        ID_DEVICE, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Day Counter",
            "de": "Tagzähler"
        }
    }*/
    TS_ITEM_UINT32(0x71, "pDayCount", &dev_stat.day_counter,
        ID_DEVICE, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

#if CONFIG_THINGSET_CAN
    /*{
        "title": {
            "en": "CAN Node Address",
            "de": "CAN Node-Adresse"
        }
    }*/
    TS_ITEM_UINT16(0xBE, "sCANAddress", &can_node_addr,
        ID_DEVICE, TS_ANY_R | TS_ANY_W, SUBSET_NVM),
#endif

    /*{
        "title": {
            "en": "Reset the Device",
            "de": "Gerät zurücksetzen"
        }
    }*/
    TS_FUNCTION(0xE0, "xReset", &reset_device, ID_DEVICE, TS_ANY_RW),

    /* 0xE2 reserved (previously used for bootloader-stm) */

    /*{
        "title": {
            "en": "Save data to EEPROM",
            "de": "Daten ins EEPROM schreiben"
        }
    }*/
    TS_FUNCTION(0xE1, "xStoreData", &data_storage_write, ID_DEVICE, TS_ANY_RW),

    /*{
        "title": {
            "en": "Thingset Authentication",
            "de": "Thingset Anmeldung"
        }
    }*/
    TS_FUNCTION(0xEE, "xAuth", &thingset_auth, ID_DEVICE, TS_ANY_RW),
    TS_ITEM_STRING(0xEF, "Password", auth_password, sizeof(auth_password), 0xEE, TS_ANY_RW, 0),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TS_GROUP(ID_BATTERY, "Battery", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Battery Voltage",
            "de": "Batterie-Spannung"
        }
    }*/
    TS_ITEM_FLOAT(0x31, "rMeas_V", &bat_bus.voltage, 2,
        ID_BATTERY, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Battery Current",
            "de": "Batterie-Strom"
        }
    }*/
    TS_ITEM_FLOAT(0x32, "rMeas_A", &bat_terminal.current, 2,
        ID_BATTERY, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Battery Power",
            "de": "Batterie-Leistung"
        }
    }*/
    TS_ITEM_FLOAT(0x33, "rCalc_W", &bat_terminal.power, 2,
        ID_BATTERY, TS_ANY_R, 0),

#if BOARD_HAS_TEMP_BAT
    /*{
        "title": {
            "en": "Battery Temperature",
            "de": "Batterie-Temperatur"
        }
    }*/
    TS_ITEM_FLOAT(0x34, "rMeas_degC", &charger.bat_temperature, 1,
        ID_BATTERY, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "External Temperature Sensor",
            "de": "Externer Temperatursensor"
        }
    }*/
    TS_ITEM_BOOL(0x35, "rTempExt", &charger.ext_temp_sensor,
        ID_BATTERY, TS_ANY_R, 0),
#endif

    /*{
        "title": {
            "en": "State of Charge",
            "de": "Batterie-Ladezustand"
        }
    }*/
    TS_ITEM_UINT16(0x40, "rSOC_pct", &charger.soc, // output will be uint8_t
        ID_BATTERY, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Number of Batteries",
            "de": "Anzahl Batterien"
        }
    }*/
    TS_ITEM_INT16(0x53, "rNumBatteries", &lv_bus.series_multiplier,
        ID_BATTERY, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Estimated Usable Battery Capacity",
            "de": "Geschätzte nutzbare Batterie-Kapazität"
        }
    }*/
    TS_ITEM_FLOAT(0x64, "pEstUsable_Ah", &charger.usable_capacity, 1,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery State of Health",
            "de": "Batterie-Gesundheitszustand"
        }
    }*/
    TS_ITEM_UINT16(0x70, "pSOH_pct", &charger.soh,    // output will be uint8_t
        ID_BATTERY, TS_ANY_R | TS_MKR_W, 0),

    /*{
        "title": {
            "en": "Battery Peak Voltage (total)",
            "de": "Maximalspannung Batterie (gesamt)"
        }
    }*/
    TS_ITEM_FLOAT(0x74, "pMaxTotal_V", &dev_stat.battery_voltage_max, 2,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Peak Temperature (all-time)",
            "de": "Maximaltemperatur Batterie (gesamt)"
        }
    }*/
    TS_ITEM_INT16(0x78, "pMaxTotal_degC", &dev_stat.bat_temp_max,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Charged Energy (today)",
            "de": "Geladene Energie (heute)"
        }
    }*/
    TS_ITEM_FLOAT(0x69, "pChgDay_Wh", &bat_terminal.pos_energy_Wh, 2,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Charged Energy (total)",
            "de": "Energiedurchsatz Ladung (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x60, "pChgTotal_Wh", &dev_stat.bat_chg_total_Wh,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Full Charge Counter",
            "de": "Zähler Vollladezyklen"
        }
    }*/
    TS_ITEM_UINT16(0x62, "pFullChgCount", &charger.num_full_charges,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Discharged Energy (today)",
            "de": "Entladene Energie (heute)"
        }
    }*/
    TS_ITEM_FLOAT(0x6A, "pDisDay_Wh", &bat_terminal.neg_energy_Wh, 2,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Discharged Energy (total)",
            "de": "Energiedurchsatz Entladung (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x61, "pDisTotal_Wh", &dev_stat.bat_dis_total_Wh,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Deep Discharge Counter",
            "de": "Zähler Tiefentladungen"
        }
    }*/
    TS_ITEM_UINT16(0x63, "pDeepDisCount", &charger.num_deep_discharges,
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_NVM),

    /*{
        "title": {
            "en": "Discharged Battery Capacity",
            "de": "Entladene Batterie-Kapazität"
        }
    }*/
    TS_ITEM_FLOAT(0x6B, "pDis_Ah", &charger.discharged_Ah, 0,   // coulomb counter
        ID_BATTERY, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Nominal Battery Capacity",
            "de": "Nominelle Batteriekapazität"
        },
        "min": 1,
        "max": 1000
    }*/
    TS_ITEM_FLOAT(0xA0, "sNom_Ah", &bat_conf_user.nominal_capacity, 1,
        ID_BATTERY, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Internal Resistance",
            "de": "Innenwiderstand Batterie"
        }
    }*/
    TS_ITEM_FLOAT(0xB1, "sInt_Ohm", &bat_conf_user.internal_resistance, 3,
        ID_BATTERY, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Wire Resistance",
            "de": "Kabelwiderstand Batterie"
        }
    }*/
    TS_ITEM_FLOAT(0xB2, "sWire_Ohm", &bat_conf_user.wire_resistance, 3,
        ID_BATTERY, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TS_GROUP(ID_CHARGER, "Charger", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Charger State",
            "de": "Ladegerät-Zustand"
        }
    }*/
    TS_ITEM_UINT32(0x50, "rState", &charger.state,
        ID_CHARGER, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Control Target Voltage",
            "de": "Spannungs-Sollwert"
        }
    }*/
    TS_ITEM_FLOAT(0x51, "rControlTarget_V", &bat_bus.sink_voltage_intercept, 2,
        ID_CHARGER, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Control Target Current",
            "de": "Strom-Sollwert"
        }
    }*/
    TS_ITEM_FLOAT(0x52, "rControlTarget_A", &bat_terminal.pos_current_limit, 2,
        ID_CHARGER, TS_ANY_R, 0),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "DC/DC State",
            "de": "DC/DC-Zustand"
        }
    }*/
    TS_ITEM_UINT16(0x54, "rDCDCState", &dcdc.state,
        ID_CHARGER, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Enable DC/DC",
            "de": "DC/DC einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x82, "wDCDCEn", &dcdc.enable,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "DC/DC Peak Current (all-time)",
            "de": "Maximalstrom DC/DC (gesamt)"
        }
    }*/
    TS_ITEM_FLOAT(0x76, "pDCDCMaxTotal_A", &dev_stat.dcdc_current_max, 2,
        ID_CHARGER, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

#if BOARD_HAS_PWM_PORT
    /*{
        "title": {
            "en": "Enable PWM Solar Input",
            "de": "PWM Solar-Eingang einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x83, "wPWMEn", &pwm_switch.enable,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_TEMP_FETS
    /*{
        "title": {
            "en": "MOSFET Temperature",
            "de": "MOSFET-Temperatur"
        }
    }*/
    TS_ITEM_FLOAT(0x37, "rMosfet_degC", &dcdc.temp_mosfets, 1,
        ID_CHARGER, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Peak MOSFET Temperature (all-time)",
            "de": "MOSFET Maximaltemperatur (gesamt)"
        }
    }*/
    TS_ITEM_INT16(0x7A, "pMosfetMax_degC", &dev_stat.mosfet_temp_max,
        ID_CHARGER, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

    /*{
        "title": {
            "en": "Battery Maximum Charge Current (bulk)",
            "de": "Maximaler Batterie-Ladestrom (bulk)"
        },
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xA1, "sChgMax_A", &bat_conf_user.charge_current_max, 1,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Charge Voltage (topping)",
            "de": "Batterie-Ladespannung (topping)"
        },
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xA2, "sChg_V", &bat_conf_user.topping_voltage, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Topping Cut-off Current",
            "de": "Abschaltstrom Vollladung"
        },
        "min": 0.0,
        "max": 20.0
    }*/
    TS_ITEM_FLOAT(0xA3, "sChgCutoff_A", &bat_conf_user.topping_cutoff_current, 1,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Topping Time Limit",
            "de": "Zeitbregrenzung Vollladung"
        }
    }*/
    TS_ITEM_UINT32(0xA4, "sChgCutoff_s", &bat_conf_user.topping_duration,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Enable Float Charging",
            "de": "Erhaltungsladung einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xA5, "sFloatChgEn", &bat_conf_user.float_enabled,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Float Voltage",
            "de": "Spannung Erhaltungsladung"
        }
    }*/
    TS_ITEM_FLOAT(0xA6, "sFloatChg_V", &bat_conf_user.float_voltage, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Float Recharge Time",
            "de": "Wiedereinschaltdauer Erhaltungsladung"
        }
    }*/
    TS_ITEM_UINT32(0xA7, "sFloatRechg_s", &bat_conf_user.float_recharge_time,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Enable Equalization Charging",
            "de": "Ausgleichsladung einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xA8, "sEqlChgEn", &bat_conf_user.equalization_enabled,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Voltage",
            "de": "Spannung Ausgleichsladung"
        }
    }*/
    TS_ITEM_FLOAT(0xA9, "sEqlChg_V", &bat_conf_user.equalization_voltage, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Current Limit",
            "de": "Maximalstrom Ausgleichsladung"
        }
    }*/
    TS_ITEM_FLOAT(0xAA, "sEqlChg_A", &bat_conf_user.equalization_current_limit, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Duration",
            "de": "Zeitbegrenzung Ausgleichsladung"
        }
    }*/
    TS_ITEM_UINT32(0xAB, "sEqlDuration_s", &bat_conf_user.equalization_duration,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Equalization Interval",
            "de": "Max. Intervall zwischen Ausgleichsladungen"
        }
    }*/
    TS_ITEM_UINT32(0xAC, "sEqlInterval_d", &bat_conf_user.equalization_trigger_days,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Deep Discharges for Equalization",
            "de": "Max. Tiefenentladungszyklen zwischen Ausgleichsladungen"
        }
    }*/
    TS_ITEM_UINT32(0xAD, "sEqlDeepDisTrigger", &bat_conf_user.equalization_trigger_deep_cycles,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Recharge Voltage",
            "de": "Batterie-Nachladespannung"
        },
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xAE, "sRechg_V", &bat_conf_user.recharge_voltage, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Minimum Voltage",
            "de": "Batterie-Minimalspannung"
        },
        "min": 8.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xAF, "sAbsMin_V", &bat_conf_user.absolute_min_voltage, 2,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Temperature Compensation",
            "de": "Temperaturausgleich"
        }
    }*/
    TS_ITEM_FLOAT(0xB0, "sTempComp_mV_K", &bat_conf_user.temperature_compensation, 3,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Charge Temperature",
            "de": "Maximale Ladetemperatur"
        }
    }*/
    TS_ITEM_FLOAT(0xB3, "sChgMax_degC", &bat_conf_user.charge_temp_max, 1,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Minimum Charge Temperature",
            "de": "Minimale Ladetemperatur"
        }
    }*/
    TS_ITEM_FLOAT(0xB4, "sChgMin_degC", &bat_conf_user.charge_temp_min, 1,
        ID_CHARGER, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "DC/DC minimum output power w/o shutdown",
            "de": "DC/DC Mindest-Leistung vor Abschaltung"
        }
    }*/
    TS_ITEM_FLOAT(0xD0, "sDcdcMin_W", &dcdc.output_power_min, 1,
        ID_CHARGER, TS_MKR_RW, SUBSET_NVM),

    /*{
        "title": {
            "en": "DC/DC Restart Interval",
            "de": "DC/DC Restart Intervall"
        }
    }*/
    TS_ITEM_UINT32(0xD2, "sDcdcRestart_s", &dcdc.restart_interval,
        ID_CHARGER, TS_MKR_RW, SUBSET_NVM),
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////////

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR

    TS_GROUP(ID_SOLAR, "Solar", TS_NO_CALLBACK, ID_ROOT),

#if CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Voltage",
            "de": "Solar-Spannung"
        }
    }*/
    TS_ITEM_FLOAT(0x38, "rMeas_V", &pwm_switch.ext_voltage, 2,
        ID_SOLAR, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#else
    TS_ITEM_FLOAT(0x38, "rMeas_V", &solar_bus.voltage, 2,
        ID_SOLAR, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

    /*{
        "title": {
            "en": "Solar Current",
            "de": "Solar-Strom"
        }
    }*/
    TS_ITEM_FLOAT(0x39, "rMeas_A", &solar_terminal.current, 2,
        ID_SOLAR, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Solar Power",
            "de": "Solar-Leistung"
        }
    }*/
    TS_ITEM_FLOAT(0x3A, "rCalc_W", &solar_terminal.power, 2,
        ID_SOLAR, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Solar Energy (today)",
            "de": "Solar-Ertrag (heute)"
        }
    }*/
    TS_ITEM_FLOAT(0x6C, "pInDay_Wh", &solar_terminal.neg_energy_Wh, 2,
        ID_SOLAR, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Solar Energy (total)",
            "de": "Solar-Energie (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x65, "pInTotal_Wh", &dev_stat.solar_in_total_Wh,
        ID_SOLAR, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Peak Solar Power (today)",
            "de": "Maximale Solarleistung (heute)"
        }
    }*/
    TS_ITEM_UINT16(0x6E, "pMaxDay_W", &dev_stat.solar_power_max_day,
        ID_SOLAR, TS_ANY_R | TS_MKR_W, 0),

    /*{
        "title": {
            "en": "Solar Peak Power (total)",
            "de": "Maximalleistung Solar (gesamt)"
        }
    }*/
    TS_ITEM_UINT16(0x72, "pMaxTotal_W", &dev_stat.solar_power_max_total,
        ID_SOLAR, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Solar Peak Voltage (all-time)",
            "de": "Maximalspannung Solar (gesamt)"
        }
    }*/
    TS_ITEM_FLOAT(0x75, "pMaxTotal_V", &dev_stat.solar_voltage_max, 2,
        ID_SOLAR, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "Absolute Maximum Solar Voltage",
            "de": "Maximal erlaubte Solar-Spannung"
        }
    }*/
    TS_ITEM_FLOAT(0xD1, "sSolarAbsMax_V", &dcdc.hs_voltage_max, 1,
        ID_SOLAR, TS_MKR_RW, SUBSET_NVM),
#endif /* BOARD_HAS_DCDC */

#endif /* SOLAR */

    ///////////////////////////////////////////////////////////////////////////////////////////////

#if BOARD_HAS_LOAD_OUTPUT

    TS_GROUP(ID_LOAD, "Load", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Load Outupt Current",
            "de": "Lastausgangs-Strom"
        }
    }*/
    TS_ITEM_FLOAT(0x3B, "rMeas_A", &load.current, 2,
        ID_LOAD, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Load Output Power",
            "de": "Lastausgangs-Leistung"
        }
    }*/
    TS_ITEM_FLOAT(0x3C, "rCalc_W", &load.power, 2,
        ID_LOAD, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Load State",
            "de": "Last-Zustand"
        }
    }*/
    TS_ITEM_INT32(0x55, "rState", &load.info,
        ID_LOAD, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Load Output Energy (today)",
            "de": "Energie Last-Ausgang (heute)"
        }
    }*/
    TS_ITEM_FLOAT(0x6D, "pOutDay_Wh", &load.pos_energy_Wh, 2,
        ID_LOAD, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Load Output Energy (total)",
            "de": "Energiedurchsatz Lastausgang (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x66, "pOutTotal_Wh", &dev_stat.load_out_total_Wh,
        ID_LOAD, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Peak Load Power (today)",
            "de": "Maximale Lastleistung (heute)"
        }
    }*/
    TS_ITEM_UINT16(0x6F, "pMaxDay_W", &dev_stat.load_power_max_day,
        ID_LOAD, TS_ANY_R | TS_MKR_W, 0),

    /*{
        "title": {
            "en": "Load Peak Power (total)",
            "de": "Maximalleistung Last-Ausgang (gesamt)"
        }
    }*/
    TS_ITEM_UINT16(0x73, "pMaxTotal_W", &dev_stat.load_power_max_total,
        ID_LOAD, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Load Peak Current (all-time)",
            "de": "Maximalstrom Lastausgang (gesamt)"
        }
    }*/
    TS_ITEM_FLOAT(0x77, "pMaxTotal_A", &dev_stat.load_current_max, 2,
        ID_LOAD, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Enable Load",
            "de": "Last einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x80, "wEnable", &load.enable,
        ID_LOAD, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "Automatic Load Output Enable",
            "de": "Last-Ausgang automatisch einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xB7, "sEnableDefault", &load.enable,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Load Disconnect Voltage ",
            "de": "Abschaltspannung Lastausgang"
        }
    }*/
    TS_ITEM_FLOAT(0xB8, "sDisconnect_V", &bat_conf_user.load_disconnect_voltage, 2,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Load Reconnect Voltage",
            "de": "Wiedereinschalt-Spannung Lastausgang"
        }
    }*/
    TS_ITEM_FLOAT(0xB9, "sReconnect_V", &bat_conf_user.load_reconnect_voltage, 2,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Overcurrent Recovery Delay",
            "de": "Wiedereinschalt-Verzögerung nach Überstrom"
        }
    }*/
    TS_ITEM_UINT32(0xBA, "sOvercurrentRecovery_s", &load.oc_recovery_delay,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Low Voltage Disconnect Recovery Delay",
            "de": "Wiedereinschalt-Verzögerung nach Unterspannung"
        }
    }*/
    TS_ITEM_UINT32(0xBB, "sUndervoltageRecovery_s", &load.lvd_recovery_delay,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Discharge Temperature",
            "de": "Maximale Entladetemperatur"
        }
    }*/
    TS_ITEM_FLOAT(0xB5, "sDisMax_degC", &bat_conf_user.discharge_temp_max, 1,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Minimum Discharge Temperature",
            "de": "Minimale Entladetemperatur"
        }
    }*/
    TS_ITEM_FLOAT(0xB6, "sDisMin_degC", &bat_conf_user.discharge_temp_min, 1,
        ID_LOAD, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

#endif /* BOARD_HAS_LOAD_OUTPUT */

    ///////////////////////////////////////////////////////////////////////////////////////////////

#if BOARD_HAS_USB_OUTPUT

    TS_GROUP(ID_USB, "USB", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "USB State",
            "de": "USB-Zustand"
        }
    }*/
    TS_ITEM_INT32(0x56, "rState", &usb_pwr.info,
        ID_USB, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Enable USB",
            "de": "USB einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x81, "wEnable", &usb_pwr.enable,
        ID_USB, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "Automatic USB Power Output Enable",
            "de": "USB Ladeport automatisch einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xBC, "sEnableDefault", &usb_pwr.enable,
        ID_USB, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "USB low voltage disconnect recovery delay",
            "de": "Wiedereinschalt-Verzögerung USB nach Unterspannung"
        }
    }*/
    TS_ITEM_UINT32(0xBD, "sUndervoltageRecovery_s", &usb_pwr.lvd_recovery_delay,
        ID_USB, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

#endif /* BOARD_HAS_USB_OUTPUT */

    ///////////////////////////////////////////////////////////////////////////////////////////////

#if CONFIG_HV_TERMINAL_NANOGRID

    TS_GROUP(ID_NANOGRID, "Nanogrid", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "DC Grid Voltage",
            "de": "Spannung DC-Netz"
        }
    }*/
    TS_ITEM_FLOAT(0x3D, "rMeas_V", &hv_bus.voltage, 2,
        ID_NANOGRID, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "DC Grid Current",
            "de": "Strom DC-Netz"
        }
    }*/
    TS_ITEM_FLOAT(0x3E, "rMeas_A", &hv_terminal.current, 2,
        ID_NANOGRID, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "DC Grid Power",
            "de": "Leistung DC-Grid"
        }
    }*/
    TS_ITEM_FLOAT(0x3F, "rCalc_W", &hv_terminal.power, 2,
        ID_NANOGRID, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Grid Imported Energy (total)",
            "de": "Energie-Import DC-Netz (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x67, "pImportTotal_Wh", &dev_stat.grid_import_total_Wh,
        ID_NANOGRID, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Grid Exported Energy (total)",
            "de": "Energie-Export DC-Netz (gesamt)"
        }
    }*/
    TS_ITEM_UINT32(0x68, "pExportTotal_Wh", &dev_stat.grid_export_total_Wh,
        ID_NANOGRID, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "DC Grid Export Voltage",
            "de": "DC-Grid Export-Spannung"
        }
    }*/
    TS_ITEM_FLOAT(0x84, "wGridSink_V", &hv_bus.sink_voltage_intercept, 2,
        ID_NANOGRID, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "DC Grid Import Voltage",
            "de": "DC-Grid Import-Spannung"
        }
    }*/
    TS_ITEM_FLOAT(0x85, "wGridSrc_V", &hv_bus.src_voltage_intercept, 2,
        ID_NANOGRID, TS_ANY_R | TS_ANY_W, 0),

#endif /* CONFIG_HV_TERMINAL_NANOGRID */

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TS_GROUP(ID_DFU, "DFU", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Start the Bootloader",
            "de": "Bootloader starten"
        }
    }*/
    TS_FUNCTION(0xF0, "xBootloaderSTM", &start_stm32_bootloader, ID_DFU, TS_ANY_RW),

    /*{
        "title": {
            "en": "Flash Memory Size",
            "de": "Flash-Speicher Gesamtgröße"
        }
    }*/
    TS_ITEM_UINT32(0xF1, "rFlashSize_KiB", &flash_size, ID_DFU, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Flash Memory Page Size",
            "de": "Flash-Speicher Seitengröße"
        }
    }*/
    TS_ITEM_UINT32(0xF2, "rFlashPageSize_B", &flash_page_size, ID_DFU, TS_ANY_R, 0),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TS_SUBSET(0x0A, "mSerial", SUBSET_SER, ID_ROOT, TS_ANY_RW),
#if CONFIG_THINGSET_CAN
    TS_SUBSET(0x0B, "mCAN", SUBSET_CAN, ID_ROOT, TS_ANY_RW),
#endif

    TS_GROUP(ID_PUB, "_pub", TS_NO_CALLBACK, ID_ROOT),

    TS_GROUP(0x101, "mSerial", NULL, ID_PUB),

    /*{
        "title": {
            "en": "Enable/Disable serial publications",
            "de": "Serielle Publikation (de)aktivieren"
        }
    }*/
    TS_ITEM_BOOL(0x102, "Enable", &pub_serial_enable, 0x101, TS_ANY_RW, 0),

#if CONFIG_THINGSET_CAN
    TS_GROUP(0x103, "mCAN", TS_NO_CALLBACK, ID_PUB),

    /*{
        "title": {
            "en": "Enable/Disable CAN publications",
            "de": "CAN Publikation (de)aktivieren"
        }
    }*/
    TS_ITEM_BOOL(0x104, "Enable", &pub_can_enable, 0x103, TS_ANY_RW, 0),
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /*
     * Control parameters (IDs >= 0x8000)
     *
     * Temporarily choosing free IDs >= 0x7000 for testing.
     */
    TS_GROUP(ID_CTRL, "Control", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Current control target",
            "de": "Sollwert Strom-Regelung"
        }
    }*/
    TS_ITEM_FLOAT(0x7001, "zCtrlTarget_A", &charger.target_current_control, 1,
        ID_CTRL, TS_ANY_RW, SUBSET_CTRL),
};
/* clang-format on */

ThingSet ts(data_objects, sizeof(data_objects) / sizeof(ThingSetDataObject));

void data_objects_update_conf()
{
    bool changed;
    if (battery_conf_check(&bat_conf_user)) {
        LOG_INF("New config valid and activated.");
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
#if BOARD_HAS_LOAD_OUTPUT
        load.set_voltage_limits(bat_conf.load_disconnect_voltage, bat_conf.load_reconnect_voltage,
                                bat_conf.absolute_max_voltage);
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
        data_storage_write();
    }
}

void data_objects_init()
{
#ifndef UNIT_TEST
    uint8_t buf[12];
    hwinfo_get_device_id(buf, sizeof(buf));

    uint64_t id64 = crc32_ieee(buf, sizeof(buf));
    id64 += ((uint64_t)CONFIG_LIBRE_SOLAR_TYPE_ID) << 32;

    uint64_to_base32(id64, device_id, sizeof(device_id), alphabet_crockford);
#endif

    data_storage_read();
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

    if (strlen(pass_exp) == strlen(auth_password)
        && strncmp(auth_password, pass_exp, strlen(pass_exp)) == 0)
    {
        LOG_INF("Authenticated as expert user.");
        ts.set_authentication(TS_EXP_MASK | TS_USR_MASK);
    }
    else if (strlen(pass_mkr) == strlen(auth_password)
             && strncmp(auth_password, pass_mkr, strlen(pass_mkr)) == 0)
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
        out[len - i - 1] = alphabet[(in >> (i * 5)) % 32];
    }
    out[len] = '\0';
}

void update_control()
{
    charger.time_last_ctrl_msg = uptime();
}

#endif /* CUSTOM_DATA_OBJECTS_FILE */
