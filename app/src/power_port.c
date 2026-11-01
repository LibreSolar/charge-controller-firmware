/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_port.h"

#include <string.h>

void dc_bus_init(struct dc_bus *bus)
{
    memset(bus, 0, sizeof(*bus));
    bus->series_multiplier = 1;
}

void power_port_init(struct power_port *port, struct dc_bus *bus, bool assign_ref_current)
{
    memset(port, 0, sizeof(*port));
    port->bus = bus;

    if (assign_ref_current) {
        bus->ref_current = &port->current;
    }
}

void power_port_init_solar(struct power_port *port)
{
    port->neg_current_limit = -50; // derating based on max. DC/DC or PWM switch current only
    port->pos_current_limit = 0;   // no current towards solar panel allowed
}

void power_port_init_nanogrid(struct power_port *port)
{
    port->pos_current_limit = 5.0F;
    port->neg_current_limit = -5.0F;

    // 1 Ohm means 1V change of target voltage per amp, same droop for both current directions.
    port->bus->sink_droop_res = 0.1F;
    port->bus->src_droop_res = 0.1F;

    // also initialize the connected bus
    port->bus->src_voltage_intercept = 30.0F;  // starting buck mode above this point
    port->bus->sink_voltage_intercept = 28.0F; // boost mode until this voltage is reached
}

void power_port_energy_balance(struct power_port *port)
{
    // remark: timespan = 1s, so no multiplication with time necessary for energy calculation
    if (port->current >= 0.0F) {
        port->pos_energy_Wh += port->bus->voltage * port->current / 3600.0F;
    }
    else {
        port->neg_energy_Wh -= port->bus->voltage * port->current / 3600.0F;
    }
}

void power_port_update_bus_current_margins(const struct power_port *port)
{
    // charging direction of battery
    port->bus->sink_current_margin = port->pos_current_limit - port->current;

    // discharging direction of battery
    port->bus->src_current_margin = port->neg_current_limit - port->current;
}
