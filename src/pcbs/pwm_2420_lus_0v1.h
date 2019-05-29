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

#ifndef PWM_2420_LUS_0V1_H
#define PWM_2420_LUS_0V1_H

#include "mbed.h"

#define CHARGER_TYPE_PWM 1  // PWM charge controller instead of MPPT

#define PWM_TIM        3    // use TIM3 timer

#define DCDC_CURRENT_MAX 20  // PCB maximum DCDC output current
#define LOAD_CURRENT_MAX 20  // PCB maximum load switch current

#define PIN_UEXT_TX   PA_2
#define PIN_UEXT_RX   PA_3
#define PIN_UEXT_SCL  PB_6
#define PIN_UEXT_SDA  PB_7
#define PIN_UEXT_MISO PA_11
#define PIN_UEXT_MOSI PA_12
#define PIN_UEXT_SCK  PB_3
#define PIN_UEXT_SSEL PA_15

#define PIN_SWD_TX    PA_9
#define PIN_SWD_RX    PA_10

#define PIN_LOAD_DIS    PB_2
#define PIN_USB_PWR_DIS PB_5
#define PIN_I_LOAD_COMP PB_4

#define PIN_REF_I_DCDC  PA_4

#define PIN_TEMP_INT_PD PA_8

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 5

#define LED_RXTX  0     // LED1, used to indicate when sending data
#define LED_SOC_1 1     // LED2
#define LED_SOC_2 2     // LED3
#define LED_SOC_3 3     // LED4
#define LED_LOAD  4     // LED5

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 3
static const PinName led_pins[NUM_LED_PINS] = {
    //  A         B         C
       PB_13,    PB_15,    PB_14
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }, // LED1
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }, // LED2
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED3
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED4
    { PIN_HIGH,  PIN_FLOAT, PIN_LOW   }  // LED5
};

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10400.0

#define ADC_GAIN_V_BAT (132 / 12)
#define ADC_GAIN_V_SOLAR (1 + 120/12 + 120/8.2)
#define ADC_GAIN_I_LOAD (1000 / 2 / (68.0/2.2)) // amp gain: 150/2.2, resistor: 2 mOhm
#define ADC_GAIN_I_SOLAR (1000 / 2 / (68.0/2.2))

#define ADC_OFFSET_V_SOLAR (-120.0 / 8.2)        // to be multiplied with VDDA to get absolute voltage offset


// position in the array written by the DMA controller
enum {
    ADC_POS_V_BAT,      // ADC 0 (PA_0)
    ADC_POS_V_SOLAR,    // ADC 1 (PA_1)
    ADC_POS_I_LOAD,     // ADC 5 (PA_5)
    ADC_POS_I_SOLAR,    // ADC 6 (PA_6)
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
