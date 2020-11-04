/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_port.h"

void PowerPort::init_solar()
{
    neg_current_limit = -50;    // derating based on max. DC/DC or PWM switch current only
    pos_current_limit = 0;      // no current towards solar panel allowed
}

void PowerPort::init_nanogrid()
{
    pos_current_limit = 5.0;
    neg_current_limit = -5.0;

    // 1 Ohm means 1V change of target voltage per amp, same droop for both current directions.
    bus->sink_droop_res = 0.1;
    bus->src_droop_res = 0.1;

    // also initialize the connected bus
    bus->src_voltage_intercept = 30.0;          // starting buck mode above this point
    bus->sink_voltage_intercept = 28.0;         // boost mode until this voltage is reached
}

void PowerPort::energy_balance()
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (current >= 0.0F) {
        pos_energy_Wh += bus->voltage * current / 3600.0F;
    }
    else {
        neg_energy_Wh -= bus->voltage * current / 3600.0F;
    }
}

void PowerPort::update_bus_current_margins() const
{
    // charging direction of battery
    bus->sink_current_margin = pos_current_limit - current;

    // discharging direction of battery
    bus->src_current_margin = neg_current_limit - current;

    //printf("pos: %.3f neg: %.3f current: %.3f\n", pos_current_limit, neg_current_limit, current);
}
