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

#ifndef __CONFIG_LS_05_H_
#define __CONFIG_LS_05_H_

#include "mbed.h"

// DC/DC converter settings
#define PWM_FREQUENCY 70 // kHz  70 = good compromise between output ripple and efficiency
#define PWM_TIM        1 // use TIM1 timer

#define DCDC_CURRENT_MAX 12  // PCB maximum DCDC output current
#define LOAD_CURRENT_MAX 12  // PCB maximum load switch current

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

#define PIN_LED_SOC   PA_9
#define PIN_LED_SOLAR PA_10
#define PIN_LED_LOAD  PB_12

#define PIN_LOAD_DIS  PB_2

#define PIN_CAN_RX    PB_8
#define PIN_CAN_TX    PB_9

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435

#define ADC_GAIN_V_BAT (130.0 / 10 )    // battery voltage divider 120k + 10k
#define ADC_GAIN_V_SOLAR (156.8 / 6.8)  // solar voltage divider: 150k + 6.kk
#define ADC_GAIN_I_LOAD (1000 / 5 / 50) // amp gain: 50, resistor: 5 mOhm
#define ADC_GAIN_I_DCDC (1000 / 5 / 50)

// position in the array written by the DMA controller
enum {
    ADC_POS_V_BAT,      // ADC 4 (PA_4)
    ADC_POS_V_SOLAR,    // ADC 5 (PA_5)
    ADC_POS_I_LOAD,     // ADC 6 (PA_6)
    ADC_POS_I_DCDC,     // ADC 7 (PA_7)
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
    NUM_ADC_CH          // trick to get the number of enums
};

// selected ADC channels (has to match with above enum)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL4 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)

#endif
