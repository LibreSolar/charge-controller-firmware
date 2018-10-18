/* LibreSolar MPPT charge controller firmware
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

#ifndef DATA_OBJECTS_H
#define DATA_OBJECTS_H

#include <stdint.h>
#include <stdbool.h>

#include "thingset.h"
#include "pcb.h"
#include "structs.h"

extern log_data_t log_data;
extern battery_t bat;
extern dcdc_t dcdc;
extern load_output_t load;
extern dcdc_port_t hs_port;
extern dcdc_port_t ls_port;

extern BatteryConfigUser battery_config_user;
extern const dcdc_control_mode dcdc_mode; // we leave it fixed for the runtime for now

extern const data_object_t dataObjects[]; // see output_can.cpp
extern const size_t dataObjectsCount;


// stores object-ids of values to be stored in EEPROM
static const int eeprom_data_objects[] = {
    0x4002, 0x4004, 0x4006, 0x4008, 0x400B,     // V, I, T max
    0x400F, 0x4010, // energy throughput
    0x4011, 0x4012, // num full charge / deep-discharge
    0x3001, 0x3002  // switch targets
};

// stores object-ids of values to be stored in EEPROM
static const int pub_data_objects[] = {
    0x4001, 0x4003, 0x4005, 0x4007, 0x400A,     // V, I, T
    0x400D, 0x400E, // energy throughput
    0x4013  // SOC
};

#endif
