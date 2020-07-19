/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPPT_2420_HPX_H
#define MPPT_2420_HPX_H

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
//#define PIN_ADC_TEMP_FETS   PA_5

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0
/*
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

static const uint32_t adc_1_sequence[] = {
    LL_ADC_CHANNEL_12,      // V_LOW
    LL_ADC_CHANNEL_15,      // V_HIGH
    LL_ADC_CHANNEL_11,      // V_SC_SENSE
    LL_ADC_CHANNEL_4,       // V_PWM
    LL_ADC_CHANNEL_6,       // TEMP_FETS
    LL_ADC_CHANNEL_VREFINT, // VREF_MCU
    LL_ADC_CHANNEL_TEMPSENSOR_ADC1, // TEMP_MCU
};

static const uint32_t adc_2_sequence[] = {
    LL_ADC_CHANNEL_1,       // I_DCDC
    LL_ADC_CHANNEL_2,       // I_LOAD
    LL_ADC_CHANNEL_5,       // I_PWM
};
*/
#endif
