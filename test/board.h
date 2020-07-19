/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PCB_STUB_H
#define PCB_STUB_H

// PWM charge controller
#define CONFIG_PWM_TERMINAL_SOLAR DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))

// MPPT buck/boost or nanogrid mode
#define CONFIG_HV_TERMINAL_SOLAR DT_NODE_EXISTS(DT_PATH(dcdc))
#define CONFIG_HV_TERMINAL_NANOGRID 0

// battery always assumed to be at low-voltage terminal (might need to be changed for boost mode)
#define CONFIG_LV_TERMINAL_BATTERY 1

// basic battery configuration
#define BATTERY_TYPE        BAT_TYPE_GEL    ///< GEL most suitable for general batteries (see battery.h for other types)
#define BATTERY_NUM_CELLS   6               ///< For lead-acid batteries: 6 for 12V system, 12 for 24V system
#define BATTERY_CAPACITY    40              ///< Cell capacity or sum of parallel cells capacity (Ah)

#define CONFIG_DEVICE_ID 12345678

#define CONFIG_THINGSET_EXPERT_PASSWORD "expert123"
#define CONFIG_THINGSET_MAKER_PASSWORD "maker456"

// Values that are otherwise defined by Kconfig
#define CONFIG_CONTROL_FREQUENCY   10   // Hz

// pin definition only needed in adc_dma.cpp to detect if they are present on the PCB
#define PIN_ADC_TEMP_BAT

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435
#define NTC_SERIES_RESISTOR 8200.0

#endif
