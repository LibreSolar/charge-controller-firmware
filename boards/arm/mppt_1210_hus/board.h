/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPPT_1210_HUS_H
#define MPPT_1210_HUS_H

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 5

#define LED_SOC_1 0     // LED1
#define LED_SOC_2 1     // LED2
#define LED_SOC_3 2     // LED3
#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 4
#define LED_RXTX  3     // LED4, used to indicate when sending data
#define LED_LOAD  4     // LED5
#else
#define LED_LOAD  3     // LED4
#define LED_RXTX  4     // LED5, used to indicate when sending data
#endif

// LED pins and pin state configuration to switch above LEDs on
#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 4
#define NUM_LED_PINS 5
#else
#define NUM_LED_PINS 3
#endif

// defined in board definition pinmux.c
extern const char *led_ports[NUM_LED_PINS];
extern const int led_pins[NUM_LED_PINS];

#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 4
const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH, PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_LOW  }, // LED1
    { PIN_LOW,  PIN_HIGH, PIN_LOW,  PIN_LOW,  PIN_LOW  }, // LED2
    { PIN_LOW,  PIN_LOW,  PIN_HIGH, PIN_LOW,  PIN_LOW  }, // LED3
    { PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_HIGH, PIN_LOW  }, // LED4
    { PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_LOW,  PIN_HIGH }  // LED5
};
#else
static const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED1
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED2
    { PIN_HIGH,  PIN_FLOAT, PIN_LOW   }, // LED3
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }, // LED4
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }  // LED5
};
#endif

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 4
#define PIN_ADC_TEMP_FETS   PA_5
#else
#define PIN_ADC_TEMP_BAT
#endif

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0

// position in the array written by the DMA controller
#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 4
enum {
    ADC_POS_V_LOW,      // ADC 0 (PA_0)
    ADC_POS_V_HIGH,     // ADC 1 (PA_1)
    ADC_POS_TEMP_FETS,  // ADC 5 (PA_5)
    ADC_POS_I_LOAD,     // ADC 6 (PA_6)
    ADC_POS_I_DCDC,     // ADC 7 (PA_7)
#if defined(CONFIG_SOC_SERIES_STM32F0X)
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
#endif
    NUM_ADC_CH          // trick to get the number of elements
};
#else
enum {
    ADC_POS_V_LOW,      // ADC 0 (PA_0)
    ADC_POS_V_HIGH,     // ADC 1 (PA_1)
    ADC_POS_I_LOAD,     // ADC 5 (PA_5)
    ADC_POS_I_DCDC,     // ADC 6 (PA_6)
    ADC_POS_TEMP_BAT,   // ADC 7 (PA_7)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
    NUM_ADC_CH          // trick to get the number of elements
};
#endif

#define NUM_ADC_1_CH NUM_ADC_CH

// selected ADC channels (has to match with above enum)
#if defined(CONFIG_SOC_SERIES_STM32F0X)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
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
