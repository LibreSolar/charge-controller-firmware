/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <stdio.h>

#include "bat_charger.h"
#include "board.h"
#include "dcdc.h"
#include "device_status.h"
#include "load.h"
#include "power_port.h"
#include "pwm_switch.h"
#include "thingset.h"

extern struct dc_bus lv_bus;
extern struct power_port lv_terminal;

#if BOARD_HAS_DCDC
extern struct dc_bus hv_bus;
extern struct power_port hv_terminal;
extern struct dcdc dcdc;
#endif

#if BOARD_HAS_PWM_PORT
extern PwmSwitch pwm_switch;
#endif

extern struct power_port &bat_terminal;
extern struct power_port &solar_terminal;
extern struct power_port &grid_terminal;

extern struct device_status dev_stat;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;

#if BOARD_HAS_LOAD_OUTPUT
extern LoadOutput load;
#endif

#if BOARD_HAS_USB_OUTPUT
extern LoadOutput usb_pwr;
#endif

extern uint32_t timestamp;

/**
 * Perform some device setup tasks (currently only used in Zephyr)
 */
void setup();
