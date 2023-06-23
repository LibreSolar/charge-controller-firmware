/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_objects.h"

#include <soc.h>
#include <zephyr/kernel.h>

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/crc.h>

#ifdef CONFIG_SOC_FAMILY_STM32
#include <stm32_ll_utils.h>
#endif

#include <stdio.h>
#include <string.h>

#include "dcdc.h"
#include "hardware.h"
#include "helper.h"
#include "setup.h"

#include <thingset.h>
#include <thingset/sdk.h>
#include <thingset/storage.h>

LOG_MODULE_REGISTER(data_objects, CONFIG_DATA_OBJECTS_LOG_LEVEL);

// can be used to configure custom data objects in separate file instead
// (e.g. data_objects_custom.cpp)
#ifndef CONFIG_CUSTOM_DATA_OBJECTS_FILE

char manufacturer[] = "Libre Solar";
char metadata_url[] = "https://files.libre.solar/tsm/cc-" STRINGIFY(DATA_OBJECTS_VERSION) ".json";
char device_type[] = DT_PROP(DT_PATH(pcb), type);
char hardware_version[] = DT_PROP(DT_PATH(pcb), version_str);
char firmware_version[] = FIRMWARE_VERSION_ID;
char device_id[9];

#ifdef CONFIG_SOC_FAMILY_STM32
uint32_t flash_size = LL_GetFlashSize();
uint32_t flash_page_size = FLASH_PAGE_SIZE;
#else
uint32_t flash_size = 128;
uint32_t flash_page_size = 0x800;
#endif

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

/**
 * Thing Set Data Objects (see thingset.io for specification)
 */

///////////////////////////////////////////////////////////////////////////////////////////////

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_DEVICE, "Device", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "Manufacturer",
        "de": "Hersteller"
    }
}*/
THINGSET_ADD_ITEM_STRING(ID_DEVICE, 0x420, "cManufacturer", manufacturer, 0, THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Device Type",
        "de": "Gerätetyp"
    }
}*/
THINGSET_ADD_ITEM_STRING(ID_DEVICE, 0x421, "cType", device_type, 0, THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Hardware Version",
        "de": "Hardware-Version"
    }
}*/
THINGSET_ADD_ITEM_STRING(ID_DEVICE, 0x422, "cHardwareVersion", hardware_version, 0, THINGSET_ANY_R,
                         0);

/*{
    "title": {
        "en": "Firmware Version",
        "de": "Firmware-Version"
    }
}*/
THINGSET_ADD_ITEM_STRING(ID_DEVICE, 0x423, "cFirmwareVersion", firmware_version, 0, THINGSET_ANY_R,
                         0);

/*{
    "title": {
        "en": "Time since last reset",
        "de": "Zeit seit Systemstart"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_DEVICE, 0x430, "rUptime_s", &timestamp, THINGSET_ANY_R, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Error Flags",
        "de": "Fehlercode"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_DEVICE, 0x431, "rErrorFlags", &dev_stat.error_flags, THINGSET_ANY_R,
                         TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Internal Temperature",
        "de": "Interne Temperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_DEVICE, 0x436, "rIntTemp_degC", &dev_stat.internal_temp, 1,
                        THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Peak Internal Temperature (all-time)",
        "de": "Interne Maximaltemperatur (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_INT16(ID_DEVICE, 0x479, "pIntTempMax_degC", &dev_stat.int_temp_max,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Day Counter",
        "de": "Tagzähler"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_DEVICE, 0x471, "pDayCount", &dev_stat.day_counter,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Reset the Device",
        "de": "Gerät zurücksetzen"
    }
}*/
THINGSET_ADD_FN_VOID(ID_DEVICE, 0x4E0, "xReset", &reset_device, THINGSET_ANY_RW);

///////////////////////////////////////////////////////////////////////////////////////////////

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_BATTERY, "Battery", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "Battery Voltage",
        "de": "Batterie-Spannung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x531, "rVoltage_V", &bat_bus.voltage, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Battery Current",
        "de": "Batterie-Strom"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x532, "rCurrent_A", &bat_terminal.current, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Battery Power",
        "de": "Batterie-Leistung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x533, "rPower_W", &bat_terminal.power, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

#if BOARD_HAS_TEMP_BAT
/*{
    "title": {
        "en": "Battery Temperature",
        "de": "Batterie-Temperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x534, "rTemperature_degC", &charger.bat_temperature, 1,
                        THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "External Temperature Sensor",
        "de": "Externer Temperatursensor"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_BATTERY, 0x535, "rExtTempSensor", &charger.ext_temp_sensor,
                       THINGSET_ANY_R, 0);
#endif

/*{
    "title": {
        "en": "State of Charge",
        "de": "Batterie-Ladezustand"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_BATTERY, 0x540, "rSOC_pct", &charger.soc, // output will be uint8_t
                         THINGSET_ANY_R, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Number of Batteries",
        "de": "Anzahl Batterien"
    }
}*/
THINGSET_ADD_ITEM_INT16(ID_BATTERY, 0x553, "rNumBatteries", &lv_bus.series_multiplier,
                        THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Estimated Usable Battery Capacity",
        "de": "Geschätzte nutzbare Batterie-Kapazität"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x564, "pCapacityEstimation_Ah", &charger.usable_capacity, 1,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE | TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery State of Health",
        "de": "Batterie-Gesundheitszustand"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_BATTERY, 0x570, "pSOH_pct", &charger.soh, // output will be uint8_t
                         THINGSET_ANY_R | THINGSET_MFR_W, 0);

/*{
    "title": {
        "en": "Battery Peak Voltage (total)",
        "de": "Maximalspannung Batterie (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x574, "pMaxTotalVoltage_V", &dev_stat.battery_voltage_max, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Peak Temperature (all-time)",
        "de": "Maximaltemperatur Batterie (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_INT16(ID_BATTERY, 0x578, "pMaxTotalTemp_degC", &dev_stat.bat_temp_max,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Charged Energy (today)",
        "de": "Geladene Energie (heute)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x569, "pChgEnergyToday_Wh", &bat_terminal.pos_energy_Wh, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Charged Energy (total)",
        "de": "Energiedurchsatz Ladung (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_BATTERY, 0x560, "pChgEnergyTotal_Wh", &dev_stat.bat_chg_total_Wh,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Full Charge Counter",
        "de": "Zähler Vollladezyklen"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_BATTERY, 0x562, "pFullChgCount", &charger.num_full_charges,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Discharged Energy (today)",
        "de": "Entladene Energie (heute)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x56A, "pDisEnergyToday_Wh", &bat_terminal.neg_energy_Wh, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Discharged Energy (total)",
        "de": "Energiedurchsatz Entladung (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_BATTERY, 0x561, "pDisEnergyTotal_Wh", &dev_stat.bat_dis_total_Wh,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Deep Discharge Counter",
        "de": "Zähler Tiefentladungen"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_BATTERY, 0x563, "pDeepDisCount", &charger.num_deep_discharges,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE | TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Discharged Battery Capacity",
        "de": "Entladene Batterie-Kapazität"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x56B, "pDisCapacity_Ah", &charger.discharged_Ah, 0,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Nominal Battery Capacity",
        "de": "Nominelle Batteriekapazität"
    },
    "min": 1,
    "max": 1000
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x5A0, "sNominalCapacity_Ah", &bat_conf_user.nominal_capacity,
                        1, THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Internal Resistance",
        "de": "Innenwiderstand Batterie"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x5B1, "sIntResistance_Ohm", &bat_conf_user.internal_resistance,
                        3, THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Wire Resistance",
        "de": "Kabelwiderstand Batterie"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_BATTERY, 0x5B2, "sWireResistance_Ohm", &bat_conf_user.wire_resistance, 3,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

///////////////////////////////////////////////////////////////////////////////////////////////

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_CHARGER, "Charger", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "Charger State",
        "de": "Ladegerät-Zustand"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x650, "rState", &charger.state, THINGSET_ANY_R,
                         TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Control Target Voltage",
        "de": "Spannungs-Sollwert"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x651, "rControlTargetVoltage_V",
                        &bat_bus.sink_voltage_intercept, 2, THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Control Target Current",
        "de": "Strom-Sollwert"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x652, "rControlTargetCurrent_A",
                        &bat_terminal.pos_current_limit, 2, THINGSET_ANY_R, 0);

#if BOARD_HAS_DCDC
/*{
    "title": {
        "en": "DC/DC State",
        "de": "DC/DC-Zustand"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_CHARGER, 0x654, "rDCDCState", &dcdc.state, THINGSET_ANY_R,
                         TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Enable DC/DC",
        "de": "DC/DC einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_CHARGER, 0x682, "wDCDCEnable", &dcdc.enable,
                       THINGSET_ANY_R | THINGSET_ANY_W, 0);

/*{
    "title": {
        "en": "DC/DC Peak Current (all-time)",
        "de": "Maximalstrom DC/DC (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x676, "pDCDCMaxTotalCurrent_A", &dev_stat.dcdc_current_max, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);
#endif

#if BOARD_HAS_PWM_PORT
/*{
    "title": {
        "en": "Enable PWM Solar Input",
        "de": "PWM Solar-Eingang einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_CHARGER, 0x683, "wPWMEnable", &pwm_switch.enable,
                       THINGSET_ANY_R | THINGSET_ANY_W, 0);
#endif

#if BOARD_HAS_TEMP_FETS
/*{
    "title": {
        "en": "MOSFET Temperature",
        "de": "MOSFET-Temperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x637, "rMosfetTemp_degC", &dcdc.temp_mosfets, 1,
                        THINGSET_ANY_R, 0);

/*{
    "title": {
        "en": "Peak MOSFET Temperature (all-time)",
        "de": "MOSFET Maximaltemperatur (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_INT16(ID_CHARGER, 0x67A, "pMosfetTempMax_degC", &dev_stat.mosfet_temp_max,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);
#endif

/*{
    "title": {
        "en": "Battery Maximum Charge Current (bulk)",
        "de": "Maximaler Batterie-Ladestrom (bulk)"
    },
    "min": 10.0,
    "max": 30.0
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6A1, "sChgCurrentLimit_A", &bat_conf_user.charge_current_max,
                        1, THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Charge Voltage (topping)",
        "de": "Batterie-Ladespannung (topping)"
    },
    "min": 10.0,
    "max": 30.0
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6A2, "sChgVoltage_V", &bat_conf_user.topping_voltage, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Topping Cut-off Current",
        "de": "Abschaltstrom Vollladung"
    },
    "min": 0.0,
    "max": 20.0
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6A3, "sChgCutoffCurrent_A",
                        &bat_conf_user.topping_cutoff_current, 1, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Topping Time Limit",
        "de": "Zeitbregrenzung Vollladung"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6A4, "sChgCutoffTime_s", &bat_conf_user.topping_duration,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Enable Float Charging",
        "de": "Erhaltungsladung einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_CHARGER, 0x6A5, "sFloatChgEnable", &bat_conf_user.float_enabled,
                       THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Float Voltage",
        "de": "Spannung Erhaltungsladung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6A6, "sFloatChgVoltage_V", &bat_conf_user.float_voltage, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Float Recharge Time",
        "de": "Wiedereinschaltdauer Erhaltungsladung"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6A7, "sFloatRechgTime_s", &bat_conf_user.float_recharge_time,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Enable Equalization Charging",
        "de": "Ausgleichsladung einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_CHARGER, 0x6A8, "sEqlChgEnable", &bat_conf_user.equalization_enabled,
                       THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Equalization Voltage",
        "de": "Spannung Ausgleichsladung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6A9, "sEqlChgVoltage_V", &bat_conf_user.equalization_voltage,
                        2, THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Equalization Current Limit",
        "de": "Maximalstrom Ausgleichsladung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6AA, "sEqlChgCurrent_A",
                        &bat_conf_user.equalization_current_limit, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Equalization Duration",
        "de": "Zeitbegrenzung Ausgleichsladung"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6AB, "sEqlDuration_s", &bat_conf_user.equalization_duration,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Maximum Equalization Interval",
        "de": "Max. Intervall zwischen Ausgleichsladungen"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6AC, "sEqlMaxInterval_d",
                         &bat_conf_user.equalization_trigger_days, THINGSET_ANY_R | THINGSET_ANY_W,
                         TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Maximum Deep Discharges for Equalization",
        "de": "Max. Tiefenentladungszyklen zwischen Ausgleichsladungen"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6AD, "sEqlDeepDisTrigger",
                         &bat_conf_user.equalization_trigger_deep_cycles,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Recharge Voltage",
        "de": "Batterie-Nachladespannung"
    },
    "min": 10.0,
    "max": 30.0
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6AE, "sRechgVoltage_V", &bat_conf_user.recharge_voltage, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Battery Minimum Voltage",
        "de": "Batterie-Minimalspannung"
    },
    "min": 8.0,
    "max": 30.0
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6AF, "sAbsMinVoltage_V", &bat_conf_user.absolute_min_voltage,
                        2, THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Temperature Compensation",
        "de": "Temperaturausgleich"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6B0, "sTempCompensation_mV_K",
                        &bat_conf_user.temperature_compensation, 3, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Maximum Charge Temperature",
        "de": "Maximale Ladetemperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6B3, "sChgMaxTemp_degC", &bat_conf_user.charge_temp_max, 1,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Minimum Charge Temperature",
        "de": "Minimale Ladetemperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6B4, "sChgMinTemp_degC", &bat_conf_user.charge_temp_min, 1,
                        THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

#if BOARD_HAS_DCDC
/*{
    "title": {
        "en": "DC/DC minimum output power w/o shutdown",
        "de": "DC/DC Mindest-Leistung vor Abschaltung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_CHARGER, 0x6D0, "sDCDCMinPower_W", &dcdc.output_power_min, 1,
                        THINGSET_MFR_RW, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "DC/DC Restart Interval",
        "de": "DC/DC Restart Intervall"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_CHARGER, 0x6D2, "sDCDCRestartInterval_s", &dcdc.restart_interval,
                         THINGSET_MFR_RW, TS_SUBSET_NVM);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_SOLAR, "Solar", THINGSET_NO_CALLBACK);

#if CONFIG_PWM_TERMINAL_SOLAR
/*{
    "title": {
        "en": "Solar Voltage",
        "de": "Solar-Spannung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x738, "rVoltage_V", &pwm_switch.ext_voltage, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);
#else
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x738, "rVoltage_V", &solar_bus.voltage, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);
#endif

/*{
    "title": {
        "en": "Solar Current",
        "de": "Solar-Strom"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x739, "rCurrent_A", &solar_terminal.current, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Solar Power",
        "de": "Solar-Leistung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x73A, "rPower_W", &solar_terminal.power, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Solar Energy (today)",
        "de": "Solar-Ertrag (heute)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x76C, "pInputToday_Wh", &solar_terminal.neg_energy_Wh, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Solar Energy (total)",
        "de": "Solar-Energie (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_SOLAR, 0x765, "pInputTotal_Wh", &dev_stat.solar_in_total_Wh,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Peak Solar Power (today)",
        "de": "Maximale Solarleistung (heute)"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_SOLAR, 0x76E, "pPowerMaxToday_W", &dev_stat.solar_power_max_day,
                         THINGSET_ANY_R | THINGSET_MFR_W, 0);

/*{
    "title": {
        "en": "Solar Peak Power (total)",
        "de": "Maximalleistung Solar (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_SOLAR, 0x772, "pPowerMaxTotal_W", &dev_stat.solar_power_max_total,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Solar Peak Voltage (all-time)",
        "de": "Maximalspannung Solar (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x775, "pVoltageMaxTotal_V", &dev_stat.solar_voltage_max, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

#if BOARD_HAS_DCDC
/*{
    "title": {
        "en": "Absolute Maximum Solar Voltage",
        "de": "Maximal erlaubte Solar-Spannung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_SOLAR, 0x7D1, "pSolarVoltageLimit_V", &dcdc.hs_voltage_max, 1,
                        THINGSET_MFR_RW, TS_SUBSET_NVM);
#endif /* BOARD_HAS_DCDC */

#endif /* SOLAR */

///////////////////////////////////////////////////////////////////////////////////////////////

#if BOARD_HAS_LOAD_OUTPUT

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_LOAD, "Load", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "Load Outupt Current",
        "de": "Lastausgangs-Strom"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x83B, "rCurrent_A", &load.current, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Load Output Power",
        "de": "Lastausgangs-Leistung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x83C, "rPower_W", &load.power, 2, THINGSET_ANY_R, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Load State",
        "de": "Last-Zustand"
    }
}*/
THINGSET_ADD_ITEM_INT32(ID_LOAD, 0x855, "rState", &load.info, THINGSET_ANY_R, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Load Output Energy (today)",
        "de": "Energie Last-Ausgang (heute)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x86D, "pOutputToday_Wh", &load.pos_energy_Wh, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Load Output Energy (total)",
        "de": "Energiedurchsatz Lastausgang (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_LOAD, 0x866, "pOutputTotal_Wh", &dev_stat.load_out_total_Wh,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Peak Load Power (today)",
        "de": "Maximale Lastleistung (heute)"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_LOAD, 0x86F, "pPowerMaxToday_W", &dev_stat.load_power_max_day,
                         THINGSET_ANY_R | THINGSET_MFR_W, 0);

/*{
    "title": {
        "en": "Load Peak Power (total)",
        "de": "Maximalleistung Last-Ausgang (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT16(ID_LOAD, 0x873, "pPowerMaxTotal_W", &dev_stat.load_power_max_total,
                         THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Load Peak Current (all-time)",
        "de": "Maximalstrom Lastausgang (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x877, "pCurrentMaxTotal_A", &dev_stat.load_current_max, 2,
                        THINGSET_ANY_R | THINGSET_MFR_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Enable Load",
        "de": "Last einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_LOAD, 0x880, "wEnable", &load.enable, THINGSET_ANY_R | THINGSET_ANY_W, 0);

/*{
    "title": {
        "en": "Automatic Load Output Enable",
        "de": "Last-Ausgang automatisch einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_LOAD, 0x8B7, "sEnableDefault", &load.enable,
                       THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Load Disconnect Voltage ",
        "de": "Abschaltspannung Lastausgang"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0xB8, "sDisconnectVoltage_V",
                        &bat_conf_user.load_disconnect_voltage, 2, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Load Reconnect Voltage",
        "de": "Wiedereinschalt-Spannung Lastausgang"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x8B9, "sReconnectVoltage_V",
                        &bat_conf_user.load_reconnect_voltage, 2, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Overcurrent Recovery Delay",
        "de": "Wiedereinschalt-Verzögerung nach Überstrom"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_LOAD, 0x8BA, "sOvercurrentRecoveryDelay_s", &load.oc_recovery_delay,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Low Voltage Disconnect Recovery Delay",
        "de": "Wiedereinschalt-Verzögerung nach Unterspannung"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_LOAD, 0x8BB, "sUndervoltageRecoveryDelay_s", &load.lvd_recovery_delay,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Maximum Discharge Temperature",
        "de": "Maximale Entladetemperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x8B5, "sDisMaxTemperature_degC",
                        &bat_conf_user.discharge_temp_max, 1, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Minimum Discharge Temperature",
        "de": "Minimale Entladetemperatur"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_LOAD, 0x8B6, "sDisMinTemperature_degC",
                        &bat_conf_user.discharge_temp_min, 1, THINGSET_ANY_R | THINGSET_ANY_W,
                        TS_SUBSET_NVM);

#endif /* BOARD_HAS_LOAD_OUTPUT */

///////////////////////////////////////////////////////////////////////////////////////////////

#if BOARD_HAS_USB_OUTPUT

THINGSET_ADD_GROUP(TS_ID_ROOT, ID_USB, "USB", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "USB State",
        "de": "USB-Zustand"
    }
}*/
THINGSET_ADD_ITEM_INT32(ID_USB, 0x956, "rState", &usb_pwr.info, THINGSET_ANY_R, TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Enable USB",
        "de": "USB einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_USB, 0x981, "wEnable", &usb_pwr.enable, THINGSET_ANY_R | THINGSET_ANY_W,
                       0);

/*{
    "title": {
        "en": "Automatic USB Power Output Enable",
        "de": "USB Ladeport automatisch einschalten"
    }
}*/
THINGSET_ADD_ITEM_BOOL(ID_USB, 0x9BC, "sEnableDefault", &usb_pwr.enable,
                       THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

/*{
    "title": {
        "en": "USB low voltage disconnect recovery delay",
        "de": "Wiedereinschalt-Verzögerung USB nach Unterspannung"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_USB, 0x9BD, "sUndervoltageRecoveryDelay_s", &usb_pwr.lvd_recovery_delay,
                         THINGSET_ANY_R | THINGSET_ANY_W, TS_SUBSET_NVM);

#endif /* BOARD_HAS_USB_OUTPUT */

///////////////////////////////////////////////////////////////////////////////////////////////

#if CONFIG_HV_TERMINAL_NANOGRID

THINGSET_ADD_GROUP(ID_NANOGRID, "Nanogrid", THINGSET_NO_CALLBACK);

/*{
    "title": {
        "en": "DC Grid Voltage",
        "de": "Spannung DC-Netz"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_NANOGRID, 0xA3D, "rVoltage_V", &hv_bus.voltage, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "DC Grid Current",
        "de": "Strom DC-Netz"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_NANOGRID, 0xA3E, "rCurrent_A", &hv_terminal.current, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "DC Grid Power",
        "de": "Leistung DC-Grid"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_NANOGRID, 0xA3F, "rPower_W", &hv_terminal.power, 2, THINGSET_ANY_R,
                        TS_SUBSET_LIVE);

/*{
    "title": {
        "en": "Grid Imported Energy (total)",
        "de": "Energie-Import DC-Netz (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_NANOGRID, 0xA67, "pImportTotalEnergy_Wh",
                         &dev_stat.grid_import_total_Wh, THINGSET_ANY_R | THINGSET_MFR_W,
                         TS_SUBSET_NVM);

/*{
    "title": {
        "en": "Grid Exported Energy (total)",
        "de": "Energie-Export DC-Netz (gesamt)"
    }
}*/
THINGSET_ADD_ITEM_UINT32(ID_NANOGRID, 0xA68, "pExportTotalEnergy_Wh",
                         &dev_stat.grid_export_total_Wh, THINGSET_ANY_R | THINGSET_MFR_W,
                         TS_SUBSET_NVM);

/*{
    "title": {
        "en": "DC Grid Export Voltage",
        "de": "DC-Grid Export-Spannung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_NANOGRID, 0xA84, "wGridSinkVoltage_V", &hv_bus.sink_voltage_intercept, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, 0);

/*{
    "title": {
        "en": "DC Grid Import Voltage",
        "de": "DC-Grid Import-Spannung"
    }
}*/
THINGSET_ADD_ITEM_FLOAT(ID_NANOGRID, 0xA85, "wGridSrcVoltage_V", &hv_bus.src_voltage_intercept, 2,
                        THINGSET_ANY_R | THINGSET_ANY_W, 0);

#endif /* CONFIG_HV_TERMINAL_NANOGRID */

///////////////////////////////////////////////////////////////////////////////////////////////

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
        thingset_storage_save_queued();
    }
}

void data_objects_init()
{
    if (battery_conf_check(&bat_conf_user)) {
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
    }
    else {
        battery_conf_overwrite(&bat_conf, &bat_conf_user);
    }

    thingset_set_update_callback(&ts, TS_SUBSET_NVM, data_objects_update_conf);
}

void update_control()
{
    charger.time_last_ctrl_msg = uptime();
}

#endif /* CUSTOM_DATA_OBJECTS_FILE */
