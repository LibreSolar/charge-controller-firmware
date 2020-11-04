/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include <stdio.h>

#include "bat_charger.h"
#include "device_status.h"
#include "load.h"
#include "power_port.h"
#include "pwm_switch.h"
#include "thingset.h"

extern DcBus lv_bus;
extern PowerPort lv_terminal;

#if DT_NODE_EXISTS(DT_PATH(dcdc))
extern DcBus hv_bus;
extern PowerPort hv_terminal;
extern Dcdc dcdc;
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
extern PwmSwitch pwm_switch;
#endif

extern PowerPort &bat_terminal;
extern PowerPort &solar_terminal;
extern PowerPort &grid_terminal;

extern DeviceStatus dev_stat;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
extern LoadOutput load;
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
extern LoadOutput usb_pwr;
#endif

extern ThingSet ts;             // defined in data_objects.cpp

extern uint32_t timestamp;

/**
 * Perform some device setup tasks (currently only used in Zephyr)
 */
void setup();
