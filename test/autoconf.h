/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// PWM charge controller
#define CONFIG_PWM_TERMINAL_SOLAR 1

// MPPT buck/boost or nanogrid mode
#define CONFIG_HV_TERMINAL_SOLAR 1
#define CONFIG_HV_TERMINAL_NANOGRID 0

// battery always assumed to be at low-voltage terminal (might need to be changed for boost mode)
#define CONFIG_LV_TERMINAL_BATTERY 1

#define CONFIG_THINGSET_EXPERT_PASSWORD "expert123"
#define CONFIG_THINGSET_MAKER_PASSWORD "maker456"

// Values that are otherwise defined by Kconfig
#define CONFIG_CONTROL_FREQUENCY   10   // Hz
