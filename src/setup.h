/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include <stdio.h>

#include "bat_charger.h"
#include "device_status.h"
#include "load.h"
#include "power_port.h"
#include "pwm_switch.h"
#include "thingset.h"

extern DcBus lv_bus;
extern PowerPort lv_terminal;

#if CONFIG_HAS_DCDC_CONVERTER
extern DcBus hv_bus;
extern PowerPort hv_terminal;
extern PowerPort dcdc_lv_port;
extern Dcdc dcdc;
#endif

#if CONFIG_HAS_PWM_SWITCH
extern PwmSwitch pwm_switch;
#endif

extern PowerPort &bat_terminal;
extern PowerPort &solar_terminal;
extern PowerPort &grid_terminal;

extern DeviceStatus dev_stat;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;

extern LoadOutput load;
extern LoadOutput usb_pwr;

extern ThingSet ts;             // defined in data_objects.cpp

extern uint32_t timestamp;

/**
 * Perform some device setup tasks (currently only used in Zephyr)
 */
void setup();
