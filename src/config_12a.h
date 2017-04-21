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
#include "ChargeController.h"

#define MPPT_12A

// DC/DC converter settings
const int _pwm_frequency = 100; // kHz

const float dcdc_current_min = 0.1;  // A       --> if lower, charger is switched off
const float solar_voltage_max = 55.0; // V

// State: Standby
const float solar_voltage_offset_start = 5.0; // V  charging switched on if Vsolar > Vbat + offset
const float solar_voltage_offset_stop = 1.0;  // V  charging switched off if Vsolar < Vbat + offset

ChargingProfile profile = {
  6, // num_cells;

  // State: Standby
  60, // time_limit_recharge; // sec
  2.3, // cell_voltage_recharge; // V

  // State: CC/bulk
  10.0, // charge_current_max;  // A        PCB maximum: 20 A

  // State: CV/absorption
  2.4, // cell_voltage_max;        // max voltage per cell
  120*60, // time_limit_CV; // sec
  2.0, // current_cutoff_CV; // A

  // State: float/trickle
  true, // trickle_enabled;
  2.3, // cell_voltage_trickle;    // target voltage for trickle charging of lead-acid batteries
  30*60, // time_trickle_recharge;     // sec

  // State: equalization
  false, // equalization_enabled;
  2.5, // cell_voltage_equalization; // V
  60, // time_limit_equalization; // sec
  1.0, // current_limit_equalization; // A
  8, // equalization_trigger_time; // weeks
  10, // equalization_trigger_deep_cycles; // times

  1.95, // cell_voltage_load_disconnect;
  2.2, // cell_voltage_load_reconnect;

  // TODO
  1.0 // temperature_compensation;
};


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
#define PIN_TEMP_INT  PA_0  // not connected
#define PIN_I_LOAD  PA_6
#define PIN_I_DCDC  PA_7

#endif
