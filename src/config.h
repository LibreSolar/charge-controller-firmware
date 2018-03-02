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

#ifndef CONFIG_H
#define CONFIG_H

// DC/DC converter settings
const int _pwm_frequency = 70; // kHz  70 = good compromise between output ripple and efficiency

#define CAN_SPEED 250000    // 250 kHz
#define CAN_NODE_ID 10

// 20A board (rev. 0.6-0.8)
#define PIN_UEXT_DIS  PC_14     // starting from rev. 0.10
#define PIN_UEXT_TX   PA_2
#define PIN_UEXT_RX   PA_3
#define PIN_UEXT_SCL  PB_6
#define PIN_UEXT_SDA  PB_7
#define PIN_UEXT_MISO PB_4
#define PIN_UEXT_MOSI PB_5
#define PIN_UEXT_SCK  PB_3
#define PIN_UEXT_SSEL PC_13     // PCB rev 0.6: PA_0

#define PIN_SWD_TX    PA_9
#define PIN_SWD_RX    PA_10

#define PIN_PWM_HS    PA_8
#define PIN_PWM_LS    PB_13

#define PIN_LED_GREEN PB_14
#define PIN_LED_RED   PB_15
#define PIN_LOAD_DIS  PB_2
#define PIN_5V_OUT_EN PB_12

#define PIN_CAN_RX    PB_8
#define PIN_CAN_TX    PB_9
#define PIN_CAN_STB   PA_15

#define PIN_REF_I_DCDC PA_4

#define PIN_V_REF    PA_5
#define PIN_V_BAT    PA_6
#define PIN_V_SOLAR  PA_7
#define PIN_TEMP_BAT PA_0
#define PIN_TEMP_INT PA_1
#define PIN_I_LOAD   PB_0
#define PIN_I_DCDC   PB_1

#define PIN_EEPROM_SCL PB_10
#define PIN_EEPROM_SDA PB_11

#endif
