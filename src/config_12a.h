/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2017 Martin Jäger (www.libre.solar)
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

#ifndef CONFIG_12A_H
#define CONFIG_12A_H

#include "mbed.h"

#define MPPT_12A

// DC/DC converter settings
const int _pwm_frequency = 100; // kHz

const int charge_current_cutoff = 20;  // mA       --> if lower, charger is switched off
const int max_solar_voltage = 55000; // mV

const int num_cells = 6;

// State: Standby
const int solar_voltage_offset_start = 5000; // mV  charging switched on if Vsolar > Vbat + offset
const int solar_voltage_offset_stop = 1000;  // mV  charging switched off if Vsolar < Vbat + offset
const int time_limit_recharge = 60; // sec
const int cell_voltage_recharge = 2300; // mV

// State: CC/bulk
const int charge_current_max = 10000;  // mA        PCB maximum: 20 A

// State: CV/absorption
const int cell_voltage_max = 2400;        // max voltage per cell
const int time_limit_CV = 120; // min
const int current_cutoff_CV = 1000; //2000; // mA

// State: float/trickle
const bool trickle_enabled = true;
const int cell_voltage_trickle = 2300;    // voltage for trickle charging of lead-acid batteries
const int time_trickle_recharge = 30;  // min

// State: equalization
const bool equalization_enabled = false;
const int cell_voltage_equalization = 2500; // mV
const int time_limit_equalization = 60; // min
const int current_limit_equalization = 1000; // mA
const int equalization_trigger_time = 8; // weeks
const int equalization_trigger_deep_cycles = 10; // times

const int cell_voltage_load_disconnect = 1950;
const int cell_voltage_load_reconnect  = 2200;

/*
// typcial LiFePO4 battery
const int num_cells = 4;
const int v_cell_max = 3550;        // max voltage per cell
const int v_cell_trickle = 3550;    // voltage for trickle charging of lead-acid batteries
const int v_cell_load_disconnect = 3000;
const int v_cell_load_reconnect  = 3150;
*/
const int thermistorBetaValue = 3435;  // typical value for Semitec 103AT-5 thermistor: 3435

//----------------------------------------------------------------------------
// global variables

// 12A board (rev. 0.5)
#define PIN_UEXT_TX   PA_2
#define PIN_UEXT_RX   PA_3
#define PIN_UEXT_SCL  PB_6
#define PIN_UEXT_SDA  PB_7
#define PIN_UEXT_MISO PB_4
#define PIN_UEXT_MOSI PB_5
#define PIN_UEXT_SCK  PB_3
#define PIN_UEXT_SSEL PA_1

#define PIN_SWD_TX    PB_10
#define PIN_SWD_RX    PB_11

#define PIN_PWM_HS    PA_8
#define PIN_PWM_LS    PB_13

#define PIN_LED_GREEN PA_9
#define PIN_LED_RED   PB_12
#define PIN_LOAD_DIS  PB_2

#define PIN_REF_I_DCDC PB_0

#define PIN_V_REF   PA_0  // not connected
#define PIN_V_BAT   PA_4
#define PIN_V_SOLAR PA_5
#define PIN_V_TEMP  PA_0  // not connected
#define PIN_I_LOAD  PA_6
#define PIN_I_DCDC  PA_7

#endif
