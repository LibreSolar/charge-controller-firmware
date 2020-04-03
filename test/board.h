/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PCB_STUB_H
#define PCB_STUB_H

// PWM charge controller
#define CONFIG_PWM_TERMINAL_SOLAR CONFIG_HAS_PWM_SWITCH

// MPPT buck/boost or nanogrid mode
#define CONFIG_HV_TERMINAL_SOLAR CONFIG_HAS_DCDC_CONVERTER
#define CONFIG_HV_TERMINAL_NANOGRID 0

// battery always assumed to be at low-voltage terminal (might need to be changed for boost mode)
#define CONFIG_LV_TERMINAL_BATTERY 1

// basic battery configuration
#define BATTERY_TYPE        BAT_TYPE_GEL    ///< GEL most suitable for general batteries (see battery.h for other types)
#define BATTERY_NUM_CELLS   6               ///< For lead-acid batteries: 6 for 12V system, 12 for 24V system
#define BATTERY_CAPACITY    40              ///< Cell capacity or sum of parallel cells capacity (Ah)

#define CONFIG_DEVICE_ID 12345678

#define DEVICE_TYPE "PCB-STUB"
#define HARDWARE_VERSION "v0.1"

// specify features of charge controller
#define CONFIG_HAS_DCDC_CONVERTER  1
#define CONFIG_HAS_PWM_SWITCH      1
#define CONFIG_HAS_LOAD_OUTPUT     1
#define CONFIG_HAS_USB_PWR_OUTPUT  1

// Values that are otherwise defined by Kconfig
#define CONFIG_CONTROL_FREQUENCY   10   // Hz
#define CONFIG_MOSFET_MAX_JUNCTION_TEMP    120
#define CONFIG_INTERNAL_MAX_REFERENCE_TEMP 50
#define CONFIG_MOSFET_THERMAL_TIME_CONSTANT  5

#define PWM_FREQUENCY 50    // kHz
#define PWM_DEADTIME 230    // ns

#define PWM_CURRENT_MAX  15  // PCB maximum PWM switch (solar) current
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

//#define PIN_LOAD_DIS    0
//#define PIN_USB_PWR_DIS 0

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

#define ADC_GAIN_V_LOW  (105.6 / 5.6)   // both voltage dividers: 100k + 5.6k
#define ADC_GAIN_V_HIGH (105.6 / 5.6)
#define ADC_GAIN_V_PWM  (105.6 / 5.6)
#define ADC_GAIN_I_LOAD (1000 / 4 / 50) // amp gain: 50, resistor: 4 mOhm
#define ADC_GAIN_I_DCDC (1000 / 4 / 50)
#define ADC_GAIN_I_PWM  (1000 / 4 / 50)

#define ADC_OFFSET_V_PWM (-120.0 / 8.2)        // to be multiplied with VDDA to get absolute voltage offset

// position in the array written by the DMA controller
enum {
    ADC_POS_V_LOW,      // ADC 0 (PA_0)
    ADC_POS_V_HIGH,     // ADC 1 (PA_1)
    ADC_POS_V_PWM,      // ADC 3 (PA_2)
    ADC_POS_I_PWM,      // ADC 4 (PA_4)
    ADC_POS_I_LOAD,     // ADC 5 (PA_5)
    ADC_POS_I_DCDC,     // ADC 6 (PA_6)
    ADC_POS_TEMP_BAT,   // ADC 7 (PA_7)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
    NUM_ADC_CH          // trick to get the number of elements
};

#define NUM_ADC_1_CH NUM_ADC_CH

#endif
