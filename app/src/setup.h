/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <stdio.h>

#include "bat_charger.h"
#include "board.h"
#include "device_status.h"
#include "load.h"
#include "power_port.h"
#include "pwm_switch.h"
#include "thingset.h"

extern DcBus lv_bus;
extern PowerPort lv_terminal;

#if BOARD_HAS_DCDC
extern DcBus hv_bus;
extern PowerPort hv_terminal;
extern Dcdc dcdc;
#endif

#if BOARD_HAS_PWM_PORT
extern PwmSwitch pwm_switch;
#endif

extern PowerPort &bat_terminal;
extern PowerPort &solar_terminal;
extern PowerPort &grid_terminal;

extern DeviceStatus dev_stat;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;

#if BOARD_HAS_LOAD_OUTPUT
extern LoadOutput load;
#endif

#if BOARD_HAS_USB_OUTPUT
extern LoadOutput usb_pwr;
#endif

extern ThingSet ts; // defined in data_objects.cpp

extern uint32_t timestamp;

/**
 * Perform some device setup tasks (currently only used in Zephyr)
 */
void setup();
