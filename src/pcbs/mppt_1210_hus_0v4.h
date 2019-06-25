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

#ifndef MPPT_1210_HUS_0V4_H
#define MPPT_1210_HUS_0V4_H

#define DEVICE_TYPE "MPPT-1210-HUS"
#define HARDWARE_VERSION "v0.4"

#include "mbed.h"

// DC/DC converter settings
#define PWM_FREQUENCY 50 // kHz  50 = better for cloud solar to increase efficiency
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
#define PIN_USB_PWR_EN  PB_12

#define PIN_REF_I_DCDC  PA_4

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 5

#define LED_SOC_1 0     // LED1
#define LED_SOC_2 1     // LED2
#define LED_SOC_3 2     // LED3
#define LED_RXTX  3     // LED4, used to indicate when sending data
#define LED_LOAD  4     // LED5

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 5
static const PinName led_pins[NUM_LED_PINS] = {
    //  GND      SOC12     SOC3      RXTX      LOAD
       PB_14,    PB_13,    PB_2,     PB_11,    PB_10
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH, PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_LOW  }, // LED1
    { PIN_LOW,  PIN_HIGH, PIN_LOW,  PIN_LOW,  PIN_LOW  }, // LED2
    { PIN_LOW,  PIN_LOW,  PIN_HIGH, PIN_LOW,  PIN_LOW  }, // LED3
    { PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_HIGH, PIN_LOW  }, // LED4
    { PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_HIGH }  // LED5
};

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_FETS   PA_5

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435

#define ADC_GAIN_V_BAT (105.6 / 5.6)    // both voltage dividers: 100k + 5.6k
#define ADC_GAIN_V_SOLAR (105.6 / 5.6)
#define ADC_GAIN_I_LOAD (1000 / 4 / 50) // amp gain: 50, resistor: 4 mOhm
#define ADC_GAIN_I_DCDC (1000 / 4 / 50)

// position in the array written by the DMA controller
enum {
    ADC_POS_V_BAT,      // ADC 0 (PA_0)
    ADC_POS_V_SOLAR,    // ADC 1 (PA_1)
    ADC_POS_TEMP_FETS,  // ADC 5 (PA_5)
    ADC_POS_I_LOAD,     // ADC 6 (PA_6)
    ADC_POS_I_DCDC,     // ADC 7 (PA_7)
#if defined(STM32F0)
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
#elif defined(STM32L0)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
#endif
    NUM_ADC_CH          // trick to get the number of elements
};

// selected ADC channels (has to match with above enum)
#if defined(STM32F0)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)
#elif defined(STM32L0)
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

#endif
