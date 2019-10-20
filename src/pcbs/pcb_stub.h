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

#ifndef PCB_STUB_H
#define PCB_STUB_H

#define DEVICE_TYPE "PCB-STUB"
#define HARDWARE_VERSION "v0.1"

// specify features of charge controller
#define FEATURE_DCDC_CONVERTER  1
#define FEATURE_PWM_SWITCH      1
#define FEATURE_LOAD_OUTPUT     1

#define PWM_FREQUENCY 50    // kHz
#define PWM_DEADTIME 230    // ns
#define PWM_TIM        3    // use TIM3 timer

#define DCDC_CURRENT_MAX 20  // PCB maximum DCDC output current
#define LOAD_CURRENT_MAX 20  // PCB maximum load switch current

#define LOW_SIDE_VOLTAGE_MAX    32  // Maximum voltage at battery port (V)
#define HIGH_SIDE_VOLTAGE_MAX   55  // Maximum voltage at PV input port (V)

#define PIN_UEXT_TX   0
#define PIN_UEXT_RX   0
#define PIN_UEXT_SCL  0
#define PIN_UEXT_SDA  0
#define PIN_UEXT_MISO 0
#define PIN_UEXT_MOSI 0
#define PIN_UEXT_SCK  0
#define PIN_UEXT_SSEL 0

#define PIN_SWD_TX    0
#define PIN_SWD_RX    0

#define PIN_LOAD_DIS    0
#define PIN_USB_PWR_DIS 0

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 5

#define LED_SOC_1 0     // LED1
#define LED_SOC_2 1     // LED2
#define LED_SOC_3 2     // LED3
#define LED_LOAD  3     // LED4
#define LED_RXTX  4     // LED5, used to indicate when sending data

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 3
static const int led_pins[NUM_LED_PINS] = {
    //  A         B         C
       0,    0,    0
};
static const pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED1
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED2
    { PIN_HIGH,  PIN_FLOAT, PIN_LOW   }, // LED3
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }, // LED4
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }  // LED5
};

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 8200.0

#define ADC_GAIN_V_BAT (105.6 / 5.6)    // both voltage dividers: 100k + 5.6k
#define ADC_GAIN_V_SOLAR (105.6 / 5.6)
#define ADC_GAIN_I_LOAD (1000 / 4 / 50) // amp gain: 50, resistor: 4 mOhm
#define ADC_GAIN_I_DCDC (1000 / 4 / 50)

#define ADC_OFFSET_V_SOLAR (-120.0 / 8.2)        // to be multiplied with VDDA to get absolute voltage offset

// trick to pass unit tests for MPPT
#define ADC_POS_I_SOLAR ADC_POS_I_DCDC
#define ADC_GAIN_I_SOLAR ADC_GAIN_I_DCDC

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

#endif
