/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPPT_1210_HUS_0V4_H
#define MPPT_1210_HUS_0V4_H

#ifdef __MBED__
#include "mbed.h"
#elif defined(__ZEPHYR__)
#include <zephyr.h>
#endif

#define DEVICE_TYPE "MPPT-1210-HUS"
#define HARDWARE_VERSION "v0.4"

// specify features of charge controller
#define CONFIG_HAS_DCDC_CONVERTER  1
#define CONFIG_HAS_PWM_SWITCH      0
#define CONFIG_HAS_LOAD_OUTPUT     1

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

#ifdef __MBED__
static const PinName led_pins[NUM_LED_PINS] = {
    //  GND      SOC12     SOC3      RXTX      LOAD
       PB_14,    PB_13,    PB_2,     PB_11,    PB_10
};
#elif defined(__ZEPHYR__)
// defined in board definition pinmux.c
extern const char *led_ports[CONFIG_NUM_LED_PINS];
extern const int led_pins[CONFIG_NUM_LED_PINS];
#endif // MBED or ZEPHYR

const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
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
#define NTC_SERIES_RESISTOR 10000.0

#define ADC_GAIN_V_LOW  (105.6 / 5.6)   // both voltage dividers: 100k + 5.6k
#define ADC_GAIN_V_HIGH (105.6 / 5.6)
#define ADC_GAIN_I_LOAD (1000 / 4 / 50) // amp gain: 50, resistor: 4 mOhm
#define ADC_GAIN_I_DCDC (1000 / 4 / 50)

// position in the array written by the DMA controller
enum {
    ADC_POS_V_LOW,      // ADC 0 (PA_0)
    ADC_POS_V_HIGH,     // ADC 1 (PA_1)
    ADC_POS_TEMP_FETS,  // ADC 5 (PA_5)
    ADC_POS_I_LOAD,     // ADC 6 (PA_6)
    ADC_POS_I_DCDC,     // ADC 7 (PA_7)
#if defined(STM32F0) || defined(CONFIG_SOC_SERIES_STM32F0X)
    ADC_POS_TEMP_MCU,   // ADC 16
    ADC_POS_VREF_MCU,   // ADC 17
#elif defined(STM32L0) || defined(CONFIG_SOC_SERIES_STM32L0X)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
#endif
    NUM_ADC_CH          // trick to get the number of elements
};

// selected ADC channels (has to match with above enum)
#if defined(STM32F0) || defined(CONFIG_SOC_SERIES_STM32F0X)
#define ADC_CHSEL ( \
    ADC_CHSELR_CHSEL0 | \
    ADC_CHSELR_CHSEL1 | \
    ADC_CHSELR_CHSEL5 | \
    ADC_CHSELR_CHSEL6 | \
    ADC_CHSELR_CHSEL7 | \
    ADC_CHSELR_CHSEL16 | \
    ADC_CHSELR_CHSEL17 \
)
#elif defined(STM32L0) || defined(CONFIG_SOC_SERIES_STM32L0X)
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
