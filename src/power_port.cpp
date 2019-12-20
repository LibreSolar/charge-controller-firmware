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

    // also initialize the connected bus
    src_voltage_start = 16.0;
    src_voltage_stop = 14.0;
}

void PowerPort::init_nanogrid()
{
    neg_current_limit = -5.0;
    neg_droop_res = 0.1;            // 1 Ohm means 1V change of target voltage per amp

    pos_current_limit = 5.0;
    pos_droop_res = 0.1;           // 1 Ohm means 1V change of target voltage per amp

    // also initialize the connected bus
    src_voltage_start = 30.0;       // starting buck mode above this point
    src_voltage_stop = 30.0;        // stopping buck mode below this point

    sink_voltage_max = 28.0;     // starting idle mode above this point
    sink_voltage_min = 10.0;
}

void PowerPort::init_load(float max_load_voltage)
{
    sink_voltage_max = max_load_voltage;        // above this voltage, load is switched off
    sink_voltage_min = 0;

    // other settings are not relevant for load output
}

// must be called exactly once per second, otherwise energy calculation gets wrong
void PowerPort::energy_balance()
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (current >= 0.0F) {
        pos_energy_Wh += voltage * current / 3600.0;
    }
    else {
        neg_energy_Wh -= voltage * current / 3600.0;
    }
}

void PowerPort::pass_voltage_targets(PowerPort *port)
{
    // droop resistance added only to limits where current actually flows
    port->sink_voltage_min = sink_voltage_min;
    port->sink_voltage_max = sink_voltage_max + pos_droop_res * current;
    port->src_voltage_start = src_voltage_start + pos_droop_res * current;
    port->src_voltage_stop = src_voltage_stop + pos_droop_res * current;
}

void ports_update_current_limits(PowerPort *internal, PowerPort *p_bat, PowerPort *p_load)
{
    // charging direction of battery
    internal->pos_current_limit = p_bat->pos_current_limit + p_load->current;

    // discharging direction of battery
    internal->neg_current_limit = p_bat->neg_current_limit;

    // load output current = max. negative (discharging) battery current.
    // this statement enables or disables the load output if discharging is not allowed
    p_load->pos_current_limit = -p_bat->neg_current_limit;
}
