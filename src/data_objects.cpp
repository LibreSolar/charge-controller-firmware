/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_objects.h"

#include <zephyr.h>
#include <soc.h>

#ifndef UNIT_TEST
#include <drivers/hwinfo.h>
#include <stm32_ll_utils.h>
#include <sys/crc.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(data_objects, CONFIG_DATA_OBJECTS_LOG_LEVEL);
#endif

#include <stdio.h>
#include <string.h>

#include "data_storage.h"
#include "dcdc.h"
#include "hardware.h"
#include "setup.h"
#include "thingset.h"
#include "helper.h"

// can be used to configure custom data objects in separate file instead
// (e.g. data_objects_custom.cpp)
#ifndef CONFIG_CUSTOM_DATA_OBJECTS_FILE

const char manufacturer[] = "Libre Solar";
const char metadata_url[] =
    "https://files.libre.solar/tsm/cc-" STRINGIFY(DATA_OBJECTS_VERSION) ".json";
const char device_type[] = DT_PROP(DT_PATH(pcb), type);
const char hardware_version[] = DT_PROP(DT_PATH(pcb), version_str);
const char firmware_version[] = FIRMWARE_VERSION_ID;
uint32_t flash_size = LL_GetFlashSize();
uint32_t flash_page_size = FLASH_PAGE_SIZE;
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

bool pub_serial_enable = IS_ENABLED(CONFIG_THINGSET_SERIAL_PUB_DEFAULT);

#if CONFIG_THINGSET_CAN
bool pub_can_enable = IS_ENABLED(CONFIG_THINGSET_CAN_PUB_DEFAULT);
uint16_t can_node_addr = CONFIG_THINGSET_CAN_DEFAULT_NODE_ID;
#endif

/**
 * Thing Set Data Objects (see thingset.io for specification)
 */
static ThingSetDataObject data_objects[] = {

    /*
     * Device information (IDs >= 0x20)
     */
    TS_GROUP(ID_INFO, "info", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "ThingSet Metadata URL",
            "de": "ThingSet Metadata URL"
        }
    }*/
    TS_ITEM_STRING(0x18, "MetadataURL", metadata_url, sizeof(metadata_url),
        ID_INFO, TS_ANY_R, SUBSET_NVM),

    /*{
        "title": {
            "en": "Device ID",
            "de": "Geräte-ID"
        }
    }*/
    TS_ITEM_STRING(0x1D, "DeviceID", device_id, sizeof(device_id),
        ID_INFO, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Manufacturer",
            "de": "Hersteller"
        }
    }*/
    TS_ITEM_STRING(0x20, "Manufacturer", manufacturer, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Device Type",
            "de": "Gerätetyp"
        }
    }*/
    TS_ITEM_STRING(0x21, "DeviceType", device_type, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Hardware Version",
            "de": "Hardware-Version"
        }
    }*/
    TS_ITEM_STRING(0x22, "HardwareVersion", hardware_version, 0,
        ID_INFO, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Firmware Version",
            "de": "Firmware-Version"
        }
    }*/
    TS_ITEM_STRING(0x23, "FirmwareVersion", firmware_version, 0,
        ID_INFO, TS_ANY_R, 0),

    /*
     * Measurement data (IDs >= 0x30)
     */
    TS_GROUP(ID_MEAS, "meas", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Time since last reset",
            "de": "Zeit seit Systemstart"
        },
        "unit": "s"
    }*/
    TS_ITEM_UINT32(0x30, "Uptime_s", &timestamp,
        ID_MEAS, TS_ANY_R, SUBSET_SER),

    /*{
        "title": {
            "en": "Battery Voltage",
            "de": "Batterie-Spannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x31, "Bat_V", &bat_bus.voltage, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Battery Current",
            "de": "Batterie-Strom"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x32, "Bat_A", &bat_terminal.current, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Battery Power",
            "de": "Batterie-Leistung"
        },
        "unit": "W"
    }*/
    TS_ITEM_FLOAT(0x33, "Bat_W", &bat_terminal.power, 2,
        ID_MEAS, TS_ANY_R, 0),

#if BOARD_HAS_TEMP_FETS
    /*{
        "title": {
            "en": "Battery Temperature",
            "de": "Batterie-Temperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0x34, "Bat_degC", &charger.bat_temperature, 1,
        ID_MEAS, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "External Temperature Sensor",
            "de": "Externer Temperatursensor"
        }
    }*/
    TS_ITEM_BOOL(0x35, "BatTempExt", &charger.ext_temp_sensor,
        ID_MEAS, TS_ANY_R, 0),
#endif

    /*{
        "title": {
            "en": "Internal Temperature",
            "de": "Interne Temperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0x36, "Int_degC", &dev_stat.internal_temp, 1,
        ID_MEAS, TS_ANY_R, 0),

#if BOARD_HAS_TEMP_FETS
    /*{
        "title": {
            "en": "MOSFET Temperature",
            "de": "MOSFET-Temperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0x37, "Mosfet_degC", &dcdc.temp_mosfets, 1,
        ID_MEAS, TS_ANY_R, 0),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Voltage",
            "de": "Solar-Spannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x38, "Solar_V", &solar_bus.voltage, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#elif CONFIG_PWM_TERMINAL_SOLAR
    TS_ITEM_FLOAT(0x38, "Solar_V", &pwm_switch.ext_voltage, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Current",
            "de": "Solar-Strom"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x39, "Solar_A", &solar_terminal.current, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Solar Power",
            "de": "Solar-Leistung"
        },
        "unit": "W"
    }*/
    TS_ITEM_FLOAT(0x3A, "Solar_W", &solar_terminal.power, 2,
        ID_MEAS, TS_ANY_R, 0),
#endif

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Outupt Current",
            "de": "Lastausgangs-Strom"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x3B, "Load_A", &load.current, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Load Output Power",
            "de": "Lastausgangs-Leistung"
        },
        "unit": "W"
    }*/
    TS_ITEM_FLOAT(0x3C, "Load_W", &load.power, 2,
        ID_MEAS, TS_ANY_R, 0),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "DC Grid Voltage",
            "de": "Spannung DC-Netz"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x3D, "Grid_V", &hv_bus.voltage, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "DC Grid Current",
            "de": "Strom DC-Netz"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x3E, "Grid_A", &hv_terminal.current, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "DC Grid Power",
            "de": "Leistung DC-Grid"
        },
        "unit": "W"
    }*/
    TS_ITEM_FLOAT(0x3F, "Grid_W", &hv_terminal.power, 2,
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

    /*{
        "title": {
            "en": "State of Charge",
            "de": "Batterie-Ladezustand"
        },
        "unit": "%"
    }*/
    TS_ITEM_UINT16(0x40, "SOC_pct", &charger.soc, // output will be uint8_t
        ID_MEAS, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*
     * State data (IDs >= 0x50)
     */
    TS_GROUP(ID_STATE, "state", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Charger State",
            "de": "Ladegerät-Zustand"
        }
    }*/
    TS_ITEM_UINT32(0x50, "ChgState", &charger.state,
        ID_STATE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Charge Target Voltage",
            "de": "Ziel-Ladespannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x51, "ChgTarget_V", &bat_bus.sink_voltage_intercept, 2,
        ID_STATE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Charge Target Current",
            "de": "Ziel-Ladestrom"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x52, "ChgTarget_A", &bat_terminal.pos_current_limit, 2,
        ID_STATE, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Number of Batteries",
            "de": "Anzahl Batterien"
        }
    }*/
    TS_ITEM_INT16(0x53, "NumBatteries", &lv_bus.series_multiplier,
        ID_MEAS, TS_ANY_R, 0),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "DC/DC State",
            "de": "DC/DC-Zustand"
        }
    }*/
    TS_ITEM_UINT16(0x54, "DCDCState", &dcdc.state,
        ID_STATE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Info",
            "de": "Last-Info"
        }
    }*/
    TS_ITEM_INT32(0x55, "LoadInfo", &load.info,
        ID_STATE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "USB Info",
            "de": "USB-Info"
        }
    }*/
    TS_ITEM_INT32(0x56, "UsbInfo", &usb_pwr.info,
        ID_STATE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),
#endif

    /*{
        "title": {
            "en": "Error Flags",
            "de": "Fehlercode"
        }
    }*/
    TS_ITEM_UINT32(0x5F, "ErrorFlags", &dev_stat.error_flags,
        ID_STATE, TS_ANY_R, SUBSET_SER | SUBSET_CAN),

    /*
     * Recorded data (IDs >= 0x60)
     */
    TS_GROUP(ID_REC, "rec", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Charged Energy (total)",
            "de": "Energiedurchsatz Ladung (gesamt)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_UINT32(0x60, "BatChgTotal_Wh", &dev_stat.bat_chg_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Discharged Energy (total)",
            "de": "Energiedurchsatz Entladung (gesamt)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_UINT32(0x61, "BatDisTotal_Wh", &dev_stat.bat_dis_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Full Charge Counter",
            "de": "Zähler Vollladezyklen"
        }
    }*/
    TS_ITEM_UINT16(0x62, "FullChgCount", &charger.num_full_charges,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Deep Discharge Counter",
            "de": "Zähler Tiefentladungen"
        }
    }*/
    TS_ITEM_UINT16(0x63, "DeepDisCount", &charger.num_deep_discharges,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_NVM),

    /*{
        "title": {
            "en": "Usable Battery Capacity",
            "de": "Nutzbare Batterie-Kapazität"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_FLOAT(0x64, "BatUsable_Ah", &charger.usable_capacity, 1,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_NVM),

    /*{
        "title": {
            "en": "Solar Energy (total)",
            "de": "Solar-Energie (gesamt)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_UINT32(0x65, "SolarInTotal_Wh", &dev_stat.solar_in_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

#if BOARD_HAS_LOAD_OUTPUT
        /*{
        "title": {
            "en": "Load Output Energy (total)",
            "de": "Energiedurchsatz Lastausgang (gesamt)"
        },
        "unit": "W"
    }*/
    TS_ITEM_UINT32(0x66, "LoadOutTotal_Wh", &dev_stat.load_out_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "Grid Imported Energy (total)",
            "de": "Energie-Import DC-Netz (gesamt)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_UINT32(0x67, "GridImportTotal_Wh", &dev_stat.grid_import_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Grid Exported Energy (total)",
            "de": "Energie-Export DC-Netz (gesamt)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_UINT32(0x68, "GridExportTotal_Wh", &dev_stat.grid_export_total_Wh,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

    /*{
        "title": {
            "en": "Charged Energy (today)",
            "de": "Geladene Energie (heute)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_FLOAT(0x69, "BatChgDay_Wh", &bat_terminal.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Discharged Energy (today)",
            "de": "Entladene Energie (heute)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_FLOAT(0x6A, "BatDisDay_Wh", &bat_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

    /*{
        "title": {
            "en": "Discharged Battery Capacity",
            "de": "Entladene Batterie-Kapazität"
        },
        "unit": "Ah"
    }*/
    TS_ITEM_FLOAT(0x6B, "Dis_Ah", &charger.discharged_Ah, 0,   // coulomb counter
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_LV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    /*{
        "title": {
            "en": "Solar Energy (today)",
            "de": "Solar-Ertrag (heute)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_FLOAT(0x6C, "SolarInDay_Wh", &solar_terminal.neg_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),
#endif

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Output Energy (today)",
            "de": "Energie Last-Ausgang (heute)"
        },
        "unit": "Wh"
    }*/
    TS_ITEM_FLOAT(0x6D, "LoadOutDay_Wh", &load.pos_energy_Wh, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_SER | SUBSET_CAN),
#endif

    /*{
        "title": {
            "en": "Peak Solar Power (today)",
            "de": "Maximale Solarleistung (heute)"
        },
        "unit": "W"
    }*/
    TS_ITEM_UINT16(0x6E, "SolarMaxDay_W", &dev_stat.solar_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Peak Load Power (today)",
            "de": "Maximale Lastleistung (heute)"
        },
        "unit": "W"
    }*/
    TS_ITEM_UINT16(0x6F, "LoadMaxDay_W", &dev_stat.load_power_max_day,
        ID_REC, TS_ANY_R | TS_MKR_W, 0),
#endif

    /*{
        "title": {
            "en": "Battery State of Health",
            "de": "Batterie-Gesundheitszustand"
        },
        "unit": "%"
    }*/
    TS_ITEM_UINT16(0x70, "SOH_pct", &charger.soh,    // output will be uint8_t
        ID_REC, TS_ANY_R | TS_MKR_W, 0),

    /*{
        "title": {
            "en": "Day Counter",
            "de": "Tagzähler"
        }
    }*/
    TS_ITEM_UINT32(0x71, "DayCount", &dev_stat.day_counter,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Solar Peak Power (total)",
            "de": "Maximalleistung Solar (gesamt)"
        },
        "unit": "W"
    }*/
    TS_ITEM_UINT16(0x72, "SolarMaxTotal_W", &dev_stat.solar_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Peak Power (total)",
            "de": "Maximalleistung Last-Ausgang (gesamt)"
        },
        "unit": "W"
    }*/
    TS_ITEM_UINT16(0x73, "LoadMaxTotal_W", &dev_stat.load_power_max_total,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

    /*{
        "title": {
            "en": "Battery Peak Voltage (total)",
            "de": "Maximalspannung Batterie (gesamt)"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x74, "BatMaxTotal_V", &dev_stat.battery_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Solar Peak Voltage (all-time)",
            "de": "Maximalspannung Solar (gesamt)"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x75, "SolarMaxTotal_V", &dev_stat.solar_voltage_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "DC/DC Peak Current (all-time)",
            "de": "Maximalstrom DC/DC (gesamt)"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x76, "DcdcMaxTotal_A", &dev_stat.dcdc_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Load Peak Current (all-time)",
            "de": "Maximalstrom Lastausgang (gesamt)"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x77, "LoadMaxTotal_A", &dev_stat.load_current_max, 2,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

    /*{
        "title": {
            "en": "Battery Peak Temperature (all-time)",
            "de": "Maximaltemperatur Batterie (gesamt)"
        },
        "unit": "°C"
    }*/
    TS_ITEM_INT16(0x78, "BatMax_degC", &dev_stat.bat_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Peak Internal Temperature (all-time)",
            "de": "Interne Maximaltemperatur (gesamt)"
        },
        "unit": "°C"
    }*/
    TS_ITEM_INT16(0x79, "IntMax_degC", &dev_stat.int_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Peak MOSFET Temperature (all-time)",
            "de": "MOSFET Maximaltemperatur (gesamt)"
        },
        "unit": "°C"
    }*/
    TS_ITEM_INT16(0x7A, "MosfetMax_degC", &dev_stat.mosfet_temp_max,
        ID_REC, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*
     * Input data (IDs >= 0x80)
     */
    TS_GROUP(ID_INPUT, "input", TS_NO_CALLBACK, ID_ROOT),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Enable Load",
            "de": "Last einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x80, "LoadEn", &load.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "Enable USB",
            "de": "USB einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x81, "UsbEn", &usb_pwr.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "Enable DC/DC",
            "de": "DC/DC einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x82, "DcdcEn", &dcdc.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if BOARD_HAS_PWM_PORT
    /*{
        "title": {
            "en": "Enable PWM Solar Input",
            "de": "PWM Solar-Eingang einschalten"
        }
    }*/
    TS_ITEM_BOOL(0x83, "PwmEn", &pwm_switch.enable,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

#if CONFIG_HV_TERMINAL_NANOGRID
    /*{
        "title": {
            "en": "DC Grid Export Voltage",
            "de": "DC-Grid Export-Spannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x84, "GridSink_V", &hv_bus.sink_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),

    /*{
        "title": {
            "en": "DC Grid Import Voltage",
            "de": "DC-Grid Import-Spannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0x85, "GridSrc_V", &hv_bus.src_voltage_intercept, 2,
        ID_INPUT, TS_ANY_R | TS_ANY_W, 0),
#endif

    /*
     * Configuration data (IDs >= 0xA0)
     */
    TS_GROUP(ID_CONF, "conf", &data_objects_update_conf, ID_ROOT),

    /*{
        "title": {
            "en": "Nominal Battery Capacity",
            "de": "Nominelle Batteriekapazität"
        },
        "unit": "Ah",
        "min": 1,
        "max": 1000
    }*/
    TS_ITEM_FLOAT(0xA0, "BatNom_Ah", &bat_conf_user.nominal_capacity, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Maximum Charge Current",
            "de": "Maximaler Batterie-Ladestrom"
        },
        "unit": "A",
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xA1, "BatChgMax_A", &bat_conf_user.charge_current_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Charge Voltage",
            "de": "Batterie-Ladespannung"
        },
        "unit": "V",
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xA2, "BatChg_V", &bat_conf_user.topping_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Topping Cut-off Current",
            "de": "Abschaltstrom Vollladung"
        },
        "unit": "A",
        "min": 0.0,
        "max": 20.0
    }*/
    TS_ITEM_FLOAT(0xA3, "ToppingCutoff_A", &bat_conf_user.topping_current_cutoff, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Topping Time Limit",
            "de": "Zeitbregrenzung Vollladung"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xA4, "ToppingCutoff_s", &bat_conf_user.topping_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Enable Trickle Charging",
            "de": "Erhaltungsladung einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xA5, "TrickleEn", &bat_conf_user.trickle_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Trickle Voltage",
            "de": "Spannung Erhaltungsladung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0xA6, "Trickle_V", &bat_conf_user.trickle_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Trickle Recharge Time",
            "de": "Wiedereinschaltdauer Erhaltungsladung"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xA7, "TrickleRecharge_s", &bat_conf_user.trickle_recharge_time,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Enable Equalization Charging",
            "de": "Ausgleichsladung einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xA8, "EqlEn", &bat_conf_user.equalization_enabled,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Voltage",
            "de": "Spannung Ausgleichsladung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0xA9, "Eql_V", &bat_conf_user.equalization_voltage, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Current Limit",
            "de": "Maximalstrom Ausgleichsladung"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0xAA, "Eql_A", &bat_conf_user.equalization_current_limit, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Equalization Duration",
            "de": "Zeitbegrenzung Ausgleichsladung"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xAB, "EqlDuration_s", &bat_conf_user.equalization_duration,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Equalization Interval",
            "de": "Max. Intervall zwischen Ausgleichsladungen"
        },
        "unit": "d"
    }*/
    TS_ITEM_INT32(0xAC, "EqlInterval_d", &bat_conf_user.equalization_trigger_days,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Deep Discharges for Equalization",
            "de": "Max. Tiefenentladungszyklen zwischen Ausgleichsladungen"
        }
    }*/
    TS_ITEM_INT32(0xAD, "EqlDeepDisTrigger", &bat_conf_user.equalization_trigger_deep_cycles,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Recharge Voltage",
            "de": "Batterie-Nachladespannung"
        },
        "unit": "V",
        "min": 10.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xAE, "BatRecharge_V", &bat_conf_user.voltage_recharge, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Minimum Voltage",
            "de": "Batterie-Minimalspannung"
        },
        "unit": "V",
        "min": 8.0,
        "max": 30.0
    }*/
    TS_ITEM_FLOAT(0xAF, "BatAbsMin_V", &bat_conf_user.voltage_absolute_min, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Temperature Compensation",
            "de": "Temperaturausgleich"
        },
        "unit": "mV/K"
    }*/
    TS_ITEM_FLOAT(0xB0, "BatTempComp_mV_K", &bat_conf_user.temperature_compensation, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Internal Resistance",
            "de": "Innenwiderstand Batterie"
        },
        "unit": "Ohm"
    }*/
    TS_ITEM_FLOAT(0xB1, "BatInt_Ohm", &bat_conf_user.internal_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Battery Wire Resistance",
            "de": "Kabelwiderstand Batterie"
        },
        "unit": "Ohm"
    }*/
    TS_ITEM_FLOAT(0xB2, "BatWire_Ohm", &bat_conf_user.wire_resistance, 3,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Charge Temperature",
            "de": "Maximale Ladetemperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0xB3, "BatChgMax_degC", &bat_conf_user.charge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Minimum Charge Temperature",
            "de": "Minimale Ladetemperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0xB4, "BatChgMin_degC", &bat_conf_user.charge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Maximum Discharge Temperature",
            "de": "Maximale Entladetemperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0xB5, "BatDisMax_degC", &bat_conf_user.discharge_temp_max, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Minimum Discharge Temperature",
            "de": "Minimale Entladetemperatur"
        },
        "unit": "°C"
    }*/
    TS_ITEM_FLOAT(0xB6, "BatDisMin_degC", &bat_conf_user.discharge_temp_min, 1,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

#if BOARD_HAS_LOAD_OUTPUT
    /*{
        "title": {
            "en": "Automatic Load Output Enable",
            "de": "Last-Ausgang automatisch einschalten"
        }
    }*/
    TS_ITEM_BOOL(0xB7, "LoadEnDefault", &load.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Load Disconnect Voltage ",
            "de": "Abschaltspannung Lastausgang"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0xB8, "LoadDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Load Reconnect Voltage",
            "de": "Wiedereinschalt-Spannung Lastausgang"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0xB9, "LoadReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Overcurrent Recovery Delay",
            "de": "Wiedereinschalt-Verzögerung nach Überstrom"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xBA, "LoadOCRecovery_s", &load.oc_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Low Voltage Disconnect Recovery Delay",
            "de": "Wiedereinschalt-Verzögerung nach Unterspannung"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xBB, "LoadUVRecovery_s", &load.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),
#endif

#if BOARD_HAS_USB_OUTPUT
    /*{
        "title": {
            "en": "Automatic USB Power Output Enable",
            "de": "USB Ladeport automatisch einschalten"
        },
        "unit": "s"
    }*/
    TS_ITEM_BOOL(0xBC, "UsbEnDefault", &usb_pwr.enable,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    //TS_ITEM_FLOAT(0x56, "UsbDisconnect_V", &bat_conf_user.voltage_load_disconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    //TS_ITEM_FLOAT(0x57, "UsbReconnect_V", &bat_conf_user.voltage_load_reconnect, 2,
    //    ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "USB low voltage disconnect recovery delay",
            "de": "Wiedereinschalt-Verzögerung USB nach Unterspannung"
        },
        "unit": "s"
    }*/
    TS_ITEM_INT32(0xBD, "UsbUVRecovery_s", &usb_pwr.lvd_recovery_delay,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),
#endif

#if CONFIG_THINGSET_CAN
    /*{
        "title": {
            "en": "CAN Node Address",
            "de": "CAN Node-Adresse"
        }
    }*/
    TS_ITEM_UINT16(0xBE, "CanAddress", &can_node_addr,
        ID_CONF, TS_ANY_R | TS_ANY_W, SUBSET_NVM),
#endif

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    TS_GROUP(ID_CAL, "cal", TS_NO_CALLBACK, ID_ROOT),

#if BOARD_HAS_DCDC
    /*{
        "title": {
            "en": "DC/DC minimum output power w/o shutdown",
            "de": "DC/DC Mindest-Leistung vor Abschaltung"
        },
        "unit": "W"
    }*/
    TS_ITEM_FLOAT(0xD0, "DcdcMin_W", &dcdc.output_power_min, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "Absolute Maximum Solar Voltage",
            "de": "Maximal erlaubte Solar-Spannung"
        },
        "unit": "V"
    }*/
    TS_ITEM_FLOAT(0xD1, "SolarAbsMax_V", &dcdc.hs_voltage_max, 1,
        ID_CAL, TS_ANY_R | TS_MKR_W, SUBSET_NVM),

    /*{
        "title": {
            "en": "DC/DC Restart Interval",
            "de": "DC/DC Restart Intervall"
        },
        "unit": "s"
    }*/
    TS_ITEM_UINT32(0xD2, "DcdcRestart_s", &dcdc.restart_interval,
        ID_CAL, TS_ANY_R | TS_MKR_W, SUBSET_NVM),
#endif

    /*
     * Remote procedure calls (IDs >= 0xE0)
     */
    TS_GROUP(ID_RPC, "rpc", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Reset the Device",
            "de": "Gerät zurücksetzen"
        }
    }*/
    TS_FUNCTION(0xE0, "x-reset", &reset_device, ID_RPC, TS_ANY_RW),

    /* 0xE2 reserved (previously used for bootloader-stm) */

    /*{
        "title": {
            "en": "Save Settings to EEPROM",
            "de": "Einstellungen ins EEPROM schreiben"
        }
    }*/
    TS_FUNCTION(0xE1, "x-save-settings", &data_storage_write, ID_RPC, TS_ANY_RW),

    /*{
        "title": {
            "en": "Thingset Authentication",
            "de": "Thingset Anmeldung"
        }
    }*/
    TS_FUNCTION(0xEE, "x-auth", &thingset_auth, ID_ROOT, TS_ANY_RW),
    TS_ITEM_STRING(0xEF, "Password", auth_password, sizeof(auth_password), 0xEE, TS_ANY_RW, 0),

    // DEVICE FIRMWARE UPGRADE (DFU) //////////////////////////////////////////
    // using IDs >= 0xF0

    TS_GROUP(ID_DFU, "dfu", TS_NO_CALLBACK, ID_ROOT),

    /*{
        "title": {
            "en": "Start the Bootloader",
            "de": "Bootloader starten"
        }
    }*/
    TS_FUNCTION(0xF0, "bootloader-stm", &start_stm32_bootloader, 0x100, TS_ANY_RW),

    /*{
        "title": {
            "en": "Flash Memory Size",
            "de": "Flash-Speicher Gesamtgröße"
        },
        "unit": "KiB"
    }*/
    TS_ITEM_UINT32(0xF1, "FlashSize_KiB", &flash_size, 0x100, TS_ANY_R, 0),

    /*{
        "title": {
            "en": "Flash Memory Page Size",
            "de": "Flash-Speicher Seitengröße"
        },
        "unit": "B"
    }*/
    TS_ITEM_UINT32(0xF2, "FlashPageSize_B", &flash_page_size, 0x100, TS_ANY_R, 0),

    // PUBLICATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0x100

    TS_SUBSET(0x0A, "serial", SUBSET_SER, ID_ROOT, TS_ANY_RW),
    TS_SUBSET(0x0B, "can", SUBSET_CAN, ID_ROOT, TS_ANY_RW),

    TS_GROUP(ID_PUB, ".pub", TS_NO_CALLBACK, ID_ROOT),

    TS_GROUP(0x100, "serial", NULL, ID_PUB),

    /*{
        "title": {
            "en": "Enable/Disable serial publications",
            "de": "Serielle Publikation (de)aktivieren"
        }
    }*/
    TS_ITEM_BOOL(0x101, "Enable", &pub_serial_enable, 0xF1, TS_ANY_RW, 0),

#if CONFIG_THINGSET_CAN
    TS_GROUP(0x102, "can", TS_NO_CALLBACK, ID_PUB),

    /*{
        "title": {
            "en": "Enable/Disable CAN publications",
            "de": "CAN Publikation (de)aktivieren"
        }
    }*/
    TS_ITEM_BOOL(0x103, "Enable", &pub_can_enable, 0xF5, TS_ANY_RW, 0),
#endif

    /*
     * Control parameters (IDs >= 0x8000)
     *
     * Temporarily choosing free IDs >= 0x7000 for testing.
     */
    TS_GROUP(ID_CTRL, "ctrl", TS_NO_CALLBACK, ID_PUB),

    /*{
        "title": {
            "en": "Current control target",
            "de": "Sollwert Strom-Regelung"
        },
        "unit": "A"
    }*/
    TS_ITEM_FLOAT(0x7001, "CtrlTarget_A", &charger.target_current_control, 1,
        ID_CTRL, TS_ANY_RW, SUBSET_CTRL),
};

ThingSet ts(data_objects, sizeof(data_objects)/sizeof(ThingSetDataObject));

void data_objects_update_conf()
{
    bool changed;
    if (battery_conf_check(&bat_conf_user)) {
        LOG_INF("New config valid and activated.");
        battery_conf_overwrite(&bat_conf_user, &bat_conf, &charger);
#if BOARD_HAS_LOAD_OUTPUT
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

void update_control()
{
    charger.time_last_ctrl_msg = uptime();
}

#endif /* CUSTOM_DATA_OBJECTS_FILE */
