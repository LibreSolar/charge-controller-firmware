/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#ifndef TEST_DATA_H
#define TEST_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "thingset.h"

// info
char manufacturer[] = "Libre Solar";
static uint32_t timestamp = 12345678;

// conf
static float bat_charging_voltage = 14.4;
static float load_disconnect_voltage = 10.8;

// input
static bool enable_switch = false;

// output
static float battery_voltage = 14.1;
static float battery_current = 5.13;
static int16_t ambient_temp = 22;

// rec
static float bat_energy_hour = 32.2;
static float bat_energy_day = 123;
static int16_t ambient_temp_max_day = 28;

// pub
bool pub_serial_enable = false;
uint16_t pub_serial_interval = 1000;

bool pub_can_enable = true;
uint16_t pub_can_interval = 100;

// exec
void reset_function(void);
void auth_function(void);
char auth_password[11];


char strbuf[300];

float f32;

static uint64_t ui64;
static int64_t i64;

static uint32_t ui32;
int32_t i32;

static uint16_t ui16;
static int16_t i16;

bool b;

int32_t A[100] = {4, 2, 8, 4};
ArrayInfo int32_array = {A, sizeof(A)/sizeof(int32_t), 4, TS_T_INT32};

float B[100] = {2.27, 3.44};
ArrayInfo float32_array = {B, sizeof(B)/sizeof(float), 2, TS_T_FLOAT32};

void dummy(void);
void conf_callback(void);


/*
 * Categories / first layer node IDs
 */
#define ID_ROOT     0x00
#define ID_INFO     0x18        // read-only device information (e.g. manufacturer, device ID)
#define ID_CONF     0x30        // configurable settings
#define ID_INPUT    0x60        // input data (e.g. set-points)
#define ID_OUTPUT   0x70        // output data (e.g. measurement values)
#define ID_REC      0xA0        // recorded data (history-dependent)
#define ID_CAL      0xD0        // calibration
#define ID_EXEC     0xE0        // function call
#define ID_PUB      0xF0        // publication setup
#define ID_SUB      0xF1        // subscription setup
#define ID_LOG      0x100       // access log data

#define PUB_SER     (1U << 0)   // UART serial
#define PUB_CAN     (1U << 1)   // CAN bus
#define PUB_NVM     (1U << 2)   // data that should be stored in EEPROM

static DataNode data_nodes[] = {

    // DEVICE INFORMATION /////////////////////////////////////////////////////
    // using IDs >= 0x18

    TS_NODE_PATH(ID_INFO, "info", 0, NULL),

    TS_NODE_STRING(0x19, "Manufacturer", manufacturer, 0, ID_INFO, TS_ANY_R, 0),
    TS_NODE_UINT32(0x1A, "Timestamp_s", &timestamp, ID_INFO, TS_ANY_RW, PUB_SER),
    TS_NODE_STRING(0x1B, "DeviceID", strbuf, sizeof(strbuf), ID_INFO, TS_ANY_R | TS_MKR_W, 0),

    // CONFIGURATION //////////////////////////////////////////////////////////
    // using IDs >= 0x30 except for high priority data objects

    TS_NODE_PATH(ID_CONF, "conf", 0, &conf_callback),

    TS_NODE_FLOAT(0x31, "BatCharging_V", &bat_charging_voltage, 2, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_FLOAT(0x32, "LoadDisconnect_V", &load_disconnect_voltage, 2, ID_CONF, TS_ANY_RW, 0),

    // INPUT DATA /////////////////////////////////////////////////////////////
    // using IDs >= 0x60

    TS_NODE_PATH(ID_INPUT, "input", 0, NULL),

    TS_NODE_BOOL(0x61, "EnableCharging", &enable_switch, ID_INPUT, TS_ANY_RW, 0),

    // OUTPUT DATA ////////////////////////////////////////////////////////////
    // using IDs >= 0x70 except for high priority data objects

    TS_NODE_PATH(ID_OUTPUT, "output", 0, NULL),

    TS_NODE_FLOAT(0x71, "Bat_V", &battery_voltage, 2, ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
    TS_NODE_FLOAT(0x72, "Bat_A", &battery_current, 2, ID_OUTPUT, TS_ANY_R, PUB_SER | PUB_CAN),
    TS_NODE_INT16(0x73, "Ambient_degC", &ambient_temp, ID_OUTPUT, TS_ANY_R, PUB_SER),

    // RECORDED DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xA0

    TS_NODE_PATH(ID_REC, "rec", 0, NULL),

    TS_NODE_FLOAT(0xA1, "BatHour_kWh", &bat_energy_hour, 2, ID_REC, TS_ANY_R, 0),
    TS_NODE_FLOAT(0xA2, "BatDay_kWh", &bat_energy_day, 2, ID_REC, TS_ANY_R, 0),
    TS_NODE_INT16(0xA3, "AmbientMaxDay_degC", &ambient_temp_max_day, ID_REC, TS_ANY_R, 0),

    // CALIBRATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xD0

    TS_NODE_PATH(ID_CAL, "cal", 0, NULL),

    // FUNCTION CALLS (EXEC) //////////////////////////////////////////////////
    // using IDs >= 0xE0

    TS_NODE_PATH(ID_EXEC, "exec", 0, NULL),

    TS_NODE_EXEC(0xE1, "reset", &reset_function, ID_EXEC, TS_ANY_RW),

    TS_NODE_EXEC(0xE2, "auth", &auth_function, 0, TS_ANY_RW),
    TS_NODE_STRING(0xE3, "Password", auth_password, sizeof(auth_password), 0xE2, TS_ANY_RW, 0),

    // PUBLICATION DATA ///////////////////////////////////////////////////////
    // using IDs >= 0xF0

    TS_NODE_PATH(ID_PUB, "pub", 0, NULL),

    TS_NODE_PATH(0xF1, "serial", ID_PUB, NULL),
    TS_NODE_BOOL(0xF2, "Enable", &pub_serial_enable, 0xF1, TS_ANY_RW, 0),
    TS_NODE_UINT16(0xF3, "Interval_ms", &pub_serial_interval, 0xF1, TS_ANY_RW, 0),
    TS_NODE_PUBSUB(0xF4, "IDs", PUB_SER, 0xF1, TS_ANY_RW, 0),

    TS_NODE_PATH(0xF5, "can", ID_PUB, NULL),
    TS_NODE_BOOL(0xF6, "Enable", &pub_can_enable, 0xF5, TS_ANY_RW, 0),
    TS_NODE_UINT16(0xF7, "Interval_ms", &pub_can_interval, 0xF5, TS_ANY_RW, 0),
    TS_NODE_PUBSUB(0xF8, "IDs", PUB_CAN, 0xF5, TS_ANY_RW, 0),

    // LOGGING DATA ///////////////////////////////////////////////////////
    // using IDs >= 0x100

    TS_NODE_PATH(0x100, "log", 0, NULL),

    TS_NODE_PATH(0x110, "hourly", 0x100, NULL),
    /*
        "hourly": {
            "0h": {"Timestamp_s":23094834,"BatHour_kWh":123},
            "-1h": {"Timestamp_s":23094834,"BatHour_kWh":123}
        }
    */

    TS_NODE_PATH(0x130, "daily", 0x100, NULL),
    /*
        "daily": {
            "0d": {"Timestamp_s":23094834,"BatDay_kWh":123,"AmbientMaxDay_degC":26},
            "-1d": {"Timestamp_s":34209348,"BatDay_kWh":151,"AmbientMaxDay_degC":28}
        },
    */

    // UNIT TEST DATA ///////////////////////////////////////////////////////
    // using IDs >= 0x1000

    TS_NODE_PATH(0x1000, "test", 0, NULL),

    TS_NODE_INT32(0x4001, "i32_readonly", &i32, 0x1000, TS_ANY_R, 0),

    TS_NODE_EXEC(0x5001, "dummy", &dummy, ID_EXEC, TS_ANY_RW),

    TS_NODE_UINT64(0x6001, "ui64", &ui64, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_INT64(0x6002, "i64", &i64, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_UINT32(0x6003, "ui32", &ui32, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_INT32(0x6004, "i32", &i32, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_UINT16(0x6005, "ui16", &ui16, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_INT16(0x6006, "i16", &i16, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_FLOAT(0x6007, "f32", &f32, 2, ID_CONF, TS_ANY_RW, 0),
    TS_NODE_BOOL(0x6008, "bool", &b, ID_CONF, TS_ANY_RW, 0),

    TS_NODE_STRING(0x6009, "strbuf", strbuf, sizeof(strbuf), ID_CONF, TS_ANY_RW, 0),

    TS_NODE_FLOAT(0x600A, "f32_rounded", &f32, 0, ID_CONF, TS_ANY_RW, 0),

    TS_NODE_UINT32(0x7001, "secret_expert", &ui32, ID_CONF, TS_ANY_R | TS_EXP_W | TS_MKR_W, 0),
    TS_NODE_UINT32(0x7002, "secret_maker", &ui32, ID_CONF, TS_ANY_R | TS_MKR_W, 0),
    TS_NODE_ARRAY(0x7003, "arrayi32", &int32_array, 0, ID_CONF, TS_ANY_RW, 0),
    // data_node->detail will specify the number of decimal places for float
    TS_NODE_ARRAY(0x7004, "arrayfloat", &float32_array, 2, ID_CONF, TS_ANY_RW, 0),
};

#endif
