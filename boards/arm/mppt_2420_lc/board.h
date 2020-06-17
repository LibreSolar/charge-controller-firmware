/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MPPT_2420_LC_H
#define MPPT_2420_LC_H

#define CONFIG_CAN 1

enum pin_state_t { PIN_HIGH, PIN_LOW, PIN_FLOAT };

// assignment LED numbers on PCB to their meaning
#define NUM_LEDS 2

#define LED_PWR  0     // LED1
#define LED_LOAD 1     // LED2

// LED pins and pin state configuration to switch above LEDs on
#define NUM_LED_PINS 2

// defined in board definition pinmux.c
extern const char *led_ports[NUM_LED_PINS];
extern const int led_pins[NUM_LED_PINS];

static const enum pin_state_t led_pin_setup[NUM_LEDS][NUM_LED_PINS] = {
    { PIN_HIGH, PIN_LOW  }, // LED1
    { PIN_LOW,  PIN_HIGH }  // LED2
};

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 10000.0

#endif
