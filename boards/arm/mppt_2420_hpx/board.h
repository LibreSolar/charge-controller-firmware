/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPPT_2420_HPX_H
#define MPPT_2420_HPX_H

#include <zephyr.h>

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
#define NUM_LEDS 1

#define LED_PWR 0     // LED1

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 1

// defined in board definition pinmux.c
extern const char *led_ports[NUM_LED_PINS];
extern const int led_pins[NUM_LED_PINS];

const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH }, // LED1
};

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_FETS   PA_5

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0

#define ADC_GAIN_V_LOW  (105.6 / 5.6)
#define ADC_GAIN_V_HIGH (102.2 / 2.2)
#define ADC_GAIN_V_PWM  (1 + 120/12 + 120/8.2)

#define ADC_GAIN_I_DCDC (1000 / 2 / 20)  // amp gain: 20, resistor: 2 mOhm

// op amp gain: 68/2.2, resistor: 2 mOhm
#define ADC_GAIN_I_LOAD (1000 / 2 / (68/2.2))
#define ADC_GAIN_I_PWM  (1000 / 2 / (68/2.2))

// to be multiplied with VREF to get absolute voltage offset
#define ADC_OFFSET_V_PWM (-120.0 / 8.2)

// position in the array written by the DMA controller
enum {
    // ADC1
    ADC_POS_V_LOW,
    ADC_POS_V_HIGH,
    ADC_POS_V_SC_SENSE,
    ADC_POS_V_PWM,
    ADC_POS_TEMP_FETS,
    ADC_POS_VREF_MCU,
    ADC_POS_TEMP_MCU,
    // ADC2
    ADC_POS_I_DCDC,
    ADC_POS_I_LOAD,
    ADC_POS_I_PWM,
    NUM_ADC_CH          // trick to get the number of elements
};

// selected ADC channels (has to match with below arrays)
#define NUM_ADC_1_CH    7
#define NUM_ADC_2_CH    3

const uint32_t adc_1_sequence[] = {
    LL_ADC_CHANNEL_12,      // V_LOW
    LL_ADC_CHANNEL_15,      // V_HIGH
    LL_ADC_CHANNEL_11,      // V_SC_SENSE
    LL_ADC_CHANNEL_4,       // V_PWM
    LL_ADC_CHANNEL_6,       // TEMP_FETS
    LL_ADC_CHANNEL_VREFINT, // VREF_MCU
    LL_ADC_CHANNEL_TEMPSENSOR_ADC1, // TEMP_MCU
};

const uint32_t adc_2_sequence[] = {
    LL_ADC_CHANNEL_1,       // I_DCDC
    LL_ADC_CHANNEL_2,       // I_LOAD
    LL_ADC_CHANNEL_5,       // I_PWM
};

#endif
