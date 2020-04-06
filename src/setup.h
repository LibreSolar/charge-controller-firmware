/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST
#include <zephyr.h>
#endif

#include <stdio.h>

#include "bat_charger.h"
#include "device_status.h"
#include "load.h"
#include "power_port.h"
#include "pwm_switch.h"
#include "thingset.h"
#include "board.h"

extern DcBus lv_bus;
extern PowerPort lv_terminal;

#if DT_COMPAT_DCDC
extern DcBus hv_bus;
extern PowerPort hv_terminal;
extern PowerPort dcdc_lv_port;
extern Dcdc dcdc;
#endif

#if DT_OUTPUTS_PWM_SWITCH_PRESENT
extern PwmSwitch pwm_switch;
#endif

extern PowerPort &bat_terminal;
extern PowerPort &solar_terminal;
extern PowerPort &grid_terminal;

extern DeviceStatus dev_stat;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;

#if DT_OUTPUTS_LOAD_PRESENT
extern LoadOutput load;
#endif

#if DT_OUTPUTS_USB_PWR_PRESENT
extern LoadOutput usb_pwr;
#endif

extern ThingSet ts;             // defined in data_objects.cpp

extern uint32_t timestamp;

/**
 * Perform some device setup tasks (currently only used in Zephyr)
 */
void setup();
