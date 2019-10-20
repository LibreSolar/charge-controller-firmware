/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

#ifndef MPPT_1210_HUS_0V2_H
#define MPPT_1210_HUS_0V2_H

#define DEVICE_TYPE "MPPT-1210-HUS"
#define HARDWARE_VERSION "v0.2"

#include "mbed.h"

// specify features of charge controller
#define FEATURE_DCDC_CONVERTER  1
#define FEATURE_PWM_SWITCH      0
#define FEATURE_LOAD_OUTPUT     1

// DC/DC converter settings
#define PWM_FREQUENCY 50 // kHz  50 = better for cloud solar to increase efficiency
#define PWM_DEADTIME 300 // ns
#define PWM_TIM        1 // use TIM1 timer

#define DCDC_CURRENT_MAX 8   // PCB maximum DCDC output current
#define LOAD_CURRENT_MAX 10  // PCB maximum load switch current

#define LOW_SIDE_VOLTAGE_MAX    16  // Maximum voltage at battery port (V)
#define HIGH_SIDE_VOLTAGE_MAX   55  // Maximum voltage at PV input port (V)

#define PIN_UEXT_TX   PA_2
#define PIN_UEXT_RX   PA_3
#define PIN_UEXT_SCL  PB_6
#define PIN_UEXT_SDA  PB_7
#define PIN_UEXT_MISO PB_4
#define PIN_UEXT_MOSI PB_5
#define PIN_UEXT_SCK  PB_3
#define PIN_UEXT_SSEL PA_15     // different than 20A MPPT

#define PIN_SWD_TX    PA_9
#define PIN_SWD_RX    PA_10
/*
#define PIN_LED_PWR   PB_14   // LED1
//#define PIN_LED_DCDC  PB_15   // LED2
#define PIN_LED_RXTX  PB_15   // LED2
#define PIN_LED_LOAD  PB_12   // LED3
*/
#define PIN_LOAD_DIS    PB_2
#define PIN_USB_PWR_EN  PC_13
#define PIN_USB_PWR_FLG PC_14

#define PIN_REF_I_DCDC PA_4

#define PIN_EEPROM_SCL PB_10
#define PIN_EEPROM_SDA PB_11

#define EEPROM_24AA32

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 3

#define LED_PWR   0     // LED1
#define LED_RXTX  1     // LED2, used to indicate when sending data
#define LED_LOAD  2     // LED3

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 3
static const PinName led_pins[NUM_LED_PINS] = {
    //  PWR      RXTX      LOAD
       PB_14,    PB_15,    PB_12
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH, PIN_LOW,  PIN_LOW  }, // LED1
    { PIN_LOW,  PIN_HIGH, PIN_LOW  }, // LED2
    { PIN_LOW,  PIN_LOW,  PIN_HIGH }  // LED3
};

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435

#define ADC_GAIN_V_BAT (110.0 / 10 )    // battery voltage divider 100k + 10k
#define ADC_GAIN_V_SOLAR (105.6 / 5.6)  // solar voltage divider: 100k + 5.6k
#define ADC_GAIN_I_LOAD (1000 / 4 / (1500.0 / 22.0)) // op amp gain: 150/2.2 = 68.2, resistor: 4 mOhm
#define ADC_GAIN_I_DCDC (1000 / 4 / (1500.0 / 22.0))

// position in the array written by the DMA controller
enum {
    ADC_POS_TEMP_BAT,   // ADC 0
    ADC_POS_TEMP_FETS,  // ADC 1
    ADC_POS_V_REF,      // ADC 5
    ADC_POS_V_BAT,      // ADC 6
    ADC_POS_V_SOLAR,    // ADC 7
    ADC_POS_I_LOAD,     // ADC 8
    ADC_POS_I_DCDC,     // ADC 9
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
    NUM_ADC_CH          // trick to get the number of enums
};

// selected ADC channels (has to match with above enum)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL8 | \
    ADC_CHSELR_CHSEL9 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)

#endif
