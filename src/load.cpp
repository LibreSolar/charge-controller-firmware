/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#ifndef UNIT_TEST
#include <zephyr.h>
#endif

#include "board.h"
#include "leds.h"
#include "device_status.h"
#include "debug.h"
#include "helper.h"

extern DeviceStatus dev_stat;

LoadOutput::LoadOutput(DcBus *dc_bus, void (*drv_switch_fn)(bool), void (*drv_init_fn)()) :
    PowerPort(dc_bus),
    switch_set(drv_switch_fn)
{
    state = LOAD_STATE_OFF;

    // call driver initialization function
    drv_init_fn();

    switch_set(false);
    junction_temperature = 25;              // starting point: 25°C
    oc_recovery_delay = 5*60;               // default: 5 minutes
    lvd_recovery_delay = 60*60;             // default: 1 hour
    ov_hysteresis = 0.3;

    enable = true;  // switch on in next control() call if everything is fine
}

// this function is called more often than the state machine
void LoadOutput::control()
{
    if (state == LOAD_STATE_ON) {

        // junction temperature calculation model for overcurrent detection
        junction_temperature = junction_temperature + (
                dev_stat.internal_temp - junction_temperature +
                current * current /
                (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) *
                (CONFIG_MOSFET_MAX_JUNCTION_TEMP - CONFIG_INTERNAL_MAX_REFERENCE_TEMP)
            ) / (CONFIG_MOSFET_THERMAL_TIME_CONSTANT * CONFIG_CONTROL_FREQUENCY);

        if (junction_temperature > CONFIG_MOSFET_MAX_JUNCTION_TEMP ||
            current > LOAD_CURRENT_MAX * 2)
        {
            flags_set(&error_flags, ERR_LOAD_OVERCURRENT);
            oc_timestamp = uptime();
        }

        // negative margin means sourcing current from bus is allowed
        if (bus->src_current_margin > -0.1F) {
            flags_set(&error_flags, ERR_LOAD_BUS_SRC_CURRENT);
        }

        if (bus->voltage < bus->src_control_voltage(disconnect_voltage)) {
            flags_set(&error_flags, ERR_LOAD_SHEDDING);
            lvd_timestamp = uptime();
        }

        // long-term overvoltage (overvoltage transients are detected as an ADC alert and switch
        // off the solar input instead of the load output)
        if (bus->voltage > overvoltage || bus->voltage >
            DT_INST_0_CHARGE_CONTROLLER_LS_VOLTAGE_MAX)
        {
            ov_debounce_counter++;
            if (ov_debounce_counter > CONFIG_CONTROL_FREQUENCY) {
                // waited 1s before setting the flag
                flags_set(&error_flags, ERR_LOAD_OVERVOLTAGE);
            }
        }
        else {
            ov_debounce_counter = 0;
        }

        if (error_flags) {
            stop();
        }

        if (enable == false) {
            switch_set(false);
            state = LOAD_STATE_OFF;
        }
    }
    else {
        // load is off: check if errors are resolved and if load can be switched on

        if (flags_check(&error_flags, ERR_LOAD_SHEDDING) &&
            bus->voltage > bus->src_control_voltage(reconnect_voltage) &&
            uptime() - lvd_timestamp > lvd_recovery_delay)
        {
            flags_clear(&error_flags, ERR_LOAD_SHEDDING);
        }

        if (flags_check(&error_flags, ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP) &&
            uptime() - oc_timestamp > oc_recovery_delay)
        {
            flags_clear(&error_flags, ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP);
        }

        if (flags_check(&error_flags, ERR_LOAD_OVERVOLTAGE) &&
            bus->voltage < (overvoltage - ov_hysteresis) &&
            bus->voltage < (DT_INST_0_CHARGE_CONTROLLER_LS_VOLTAGE_MAX - ov_hysteresis))
        {
            flags_clear(&error_flags, ERR_LOAD_OVERVOLTAGE);
        }

        if (flags_check(&error_flags, ERR_LOAD_SHORT_CIRCUIT) && enable == false) {
            // stay here until the charge controller is reset or load is manually switched off
            flags_clear(&error_flags, ERR_LOAD_SHORT_CIRCUIT);
        }

        // finally switch on if all errors were resolved and at least 1A src current is available
        if (enable == true && !error_flags && bus->src_current_margin < -1.0F) {
            switch_set(true);
            state = LOAD_STATE_ON;
        }
    }

    info = error_flags > 0 ? -error_flags : state;
}

void LoadOutput::stop(uint32_t flag)
{
    switch_set(false);
    state = LOAD_STATE_OFF;
    flags_set(&error_flags, flag);

    // flicker the load LED if failure was most probably caused by the user
    if (flags_check(&error_flags,
        ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP | ERR_LOAD_SHORT_CIRCUIT))
    {
#ifdef LED_LOAD
        leds_flicker(LED_LOAD);
#endif
        oc_timestamp = uptime();
    }
}

void LoadOutput::set_voltage_limits(float lvd, float lvr, float ov)
{
    disconnect_voltage = lvd;
    reconnect_voltage = lvr;

    overvoltage = ov;
}
