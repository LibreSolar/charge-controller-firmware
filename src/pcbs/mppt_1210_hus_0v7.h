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

#ifndef MPPT_1210_HUS_0V7_H
#define MPPT_1210_HUS_0V7_H

#define DEVICE_TYPE "MPPT-1210-HUS"
#define HARDWARE_VERSION "v0.7.1"

#ifdef __MBED__
#include "mbed.h"
#endif

// specify features of charge controller
#define FEATURE_DCDC_CONVERTER  1
#define FEATURE_PWM_SWITCH      0
#define FEATURE_LOAD_OUTPUT     1

// DC/DC converter settings
#define PWM_FREQUENCY 50 // kHz  50 = better for cloud solar to increase efficiency
#define PWM_DEADTIME 230 // ns
#define PWM_TIM        3 // use TIM3 timer

#define DCDC_CURRENT_MAX 10  // PCB maximum DCDC output current
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
#define PIN_UEXT_SSEL PA_15

#define PIN_SWD_TX    PA_9
#define PIN_SWD_RX    PA_10

#define PIN_LOAD_EN     PC_13
#define PIN_USB_PWR_EN  PB_10
#define PIN_V_SOLAR_EN  PC_14
#define PIN_5V_PGOOD    PC_15

#define PIN_EXT_BTN     PB_12
#define PIN_BOOT0_EN    PB_12

#define PIN_REF_I_DCDC  PA_4

// Internal NTC temperature currently ignored by firmware as it is similar to MCU temperature and
// does not reflect external battery temperature. Feature will be removed in future HW revisions.
#define PIN_TEMP_INT_PD PA_8

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 5

#define LED_SOC_1 0     // LED1
#define LED_SOC_2 1     // LED2
#define LED_SOC_3 2     // LED3
#define LED_LOAD  3     // LED4
#define LED_RXTX  4     // LED5, used to indicate when sending data

#ifdef __MBED__
// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 3
static const PinName led_pins[NUM_LED_PINS] = {
    //  A         B         C
       PB_13,    PB_2,    PB_14
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED1
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED2
    { PIN_HIGH,  PIN_FLOAT, PIN_LOW   }, // LED3
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }, // LED4
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }  // LED5
};
#endif // __MBED__

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0

#define ADC_GAIN_V_BAT (105.6 / 5.6)    // both voltage dividers: 100k + 5.6k
#define ADC_GAIN_V_SOLAR (105.6 / 5.6)
#define ADC_GAIN_I_LOAD (1000.0 / 3.0 / 50.0) // amp gain: 50, resistor: 3 mOhm
#define ADC_GAIN_I_DCDC (1000.0 / 3.0 / 50.0)

// position in the array written by the DMA controller
enum {
    ADC_POS_V_BAT,      // ADC 0 (PA_0)
    ADC_POS_V_SOLAR,    // ADC 1 (PA_1)
    ADC_POS_I_LOAD,     // ADC 5 (PA_5)
    ADC_POS_I_DCDC,     // ADC 6 (PA_6)
    ADC_POS_TEMP_BAT,   // ADC 7 (PA_7)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
    NUM_ADC_CH          // trick to get the number of elements
};

// selected ADC channels (has to match with above enum)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL17 | \
    ADC_CHSELR_CHSEL18 \
)

#endif
