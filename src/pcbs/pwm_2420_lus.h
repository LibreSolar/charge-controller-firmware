/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PWM_2420_LUS_H
#define PWM_2420_LUS_H

#ifdef __MBED__
#include "mbed.h"
#elif defined(__ZEPHYR__)
#include <zephyr.h>
#endif

#define DEVICE_TYPE "PWM-2420-LUS"

#ifdef CONFIG_BOARD_PWM_2420_LUS_0V2
#define HARDWARE_VERSION "v0.2"
#elif defined(CONFIG_BOARD_PWM_2420_LUS_0V3)
#define HARDWARE_VERSION "v0.3"
#endif

// specify features of charge controller
#define CONFIG_HAS_DCDC_CONVERTER  0
#define CONFIG_HAS_PWM_SWITCH      1
#define CONFIG_HAS_LOAD_OUTPUT     1

#define PWM_TIM        3    // use TIM3 timer
#define PWM_FREQUENCY  50   // Hz

// Current reduced to 15A. Increase to 20A PCB max values only if attached to a big heat sink.
#define PWM_CURRENT_MAX     15  // PCB maximum PWM switch (solar) current
#define LOAD_CURRENT_MAX    15  // PCB maximum load switch current

#define LOW_SIDE_VOLTAGE_MAX    32  // Maximum voltage at battery port (V)
#define HIGH_SIDE_VOLTAGE_MAX   55  // Maximum voltage at PV input port (V)

// The MCU, where internal temperature is measured, is close to the MOSFETs. Tests showed that
// temperature at heat sink is only 10-20°C above measured internal temp. As PWM CC doesn't
// use electrolytic cap for core charging function, internal temperature of 70°C can be allowed.
// This value is used for thermal overcurrent model.
#define INTERNAL_MAX_REFERENCE_TEMP 70

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

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 3

#ifdef __MBED__
static const PinName led_pins[NUM_LED_PINS] = {
    //  A         B         C
       PB_13,    PB_14,    PB_15
};
#elif defined(__ZEPHYR__)
// defined in board definition pinmux.c
extern const char *led_ports[CONFIG_NUM_LED_PINS];
extern const int led_pins[CONFIG_NUM_LED_PINS];
#endif // MBED or ZEPHYR

#ifndef LEDS_WRONG_POLARITY
static const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED1
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED2
    { PIN_HIGH,  PIN_FLOAT, PIN_LOW   }, // LED3
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }, // LED4
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }  // LED5
};
#else
// LEDs with wrong polarity
static const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_LOW,   PIN_HIGH,  PIN_FLOAT }, // LED1
    { PIN_HIGH,  PIN_LOW,   PIN_FLOAT }, // LED2
    { PIN_LOW,   PIN_FLOAT, PIN_HIGH  }, // LED3
    { PIN_FLOAT, PIN_LOW,   PIN_HIGH  }, // LED4
    { PIN_FLOAT, PIN_HIGH,  PIN_LOW   }  // LED5
};
#endif

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 8200.0

#define ADC_GAIN_V_BAT (132 / 12)
#define ADC_GAIN_V_SOLAR (1 + 120/12 + 120/8.2)

#ifdef CONFIG_BOARD_PWM_2420_LUS_0V2
// op amp gain: 68/2.2, resistor: 2 mOhm
#define ADC_GAIN_I_LOAD (1000 / 2 / (68/2.2))
#elif defined(CONFIG_BOARD_PWM_2420_LUS_0V3)
// fix for hardware bug in overcurrent comparator voltage divider wiring
#define ADC_GAIN_I_LOAD (1000 / 2 / (68/2.2) * (39+12+8.2) / (12+8.2))
#endif

#define ADC_GAIN_I_SOLAR (1000 / 2 / (68/2.2))

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
