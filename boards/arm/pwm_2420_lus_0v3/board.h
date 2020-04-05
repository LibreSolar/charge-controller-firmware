/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PWM_2420_LUS_H
#define PWM_2420_LUS_H

#define LOAD_CURRENT_MAX    15  // PCB maximum load switch current

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

// defined in board definition pinmux.c
extern const char *led_ports[NUM_LED_PINS];
extern const int led_pins[NUM_LED_PINS];

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

#define ADC_GAIN_V_LOW (132 / 12)
#define ADC_GAIN_V_PWM (1 + 120/12 + 120/8.2)

#if DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 2
// op amp gain: 68/2.2, resistor: 2 mOhm
#define ADC_GAIN_I_LOAD (1000 / 2 / (68/2.2))
#elif DT_CHARGE_CONTROLLER_PCB_VERSION_NUM == 3
// fix for hardware bug in overcurrent comparator voltage divider wiring
#define ADC_GAIN_I_LOAD (1000 / 2 / (68/2.2) * (39+12+8.2) / (12+8.2))
#endif

#define ADC_GAIN_I_PWM (1000 / 2 / (68/2.2))

#define ADC_OFFSET_V_PWM (-120.0 / 8.2)        // to be multiplied with VDDA to get absolute voltage offset


// position in the array written by the DMA controller
enum {
    ADC_POS_V_LOW,      // ADC 0 (PA_0)
    ADC_POS_V_PWM,      // ADC 1 (PA_1)
    ADC_POS_I_LOAD,     // ADC 5 (PA_5)
    ADC_POS_I_PWM,      // ADC 6 (PA_6)
    ADC_POS_TEMP_BAT,   // ADC 7 (PA_7)
    ADC_POS_VREF_MCU,   // ADC 17
    ADC_POS_TEMP_MCU,   // ADC 18
    NUM_ADC_CH          // trick to get the number of elements
};

#define NUM_ADC_1_CH NUM_ADC_CH

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
