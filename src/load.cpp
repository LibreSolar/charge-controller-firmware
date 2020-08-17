/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#include <zephyr.h>

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load)) || UNIT_TEST

#include "leds.h"
#include "device_status.h"
#include "helper.h"

#define LOAD_CURRENT_MAX        DT_PROP(DT_CHILD(DT_PATH(outputs), load), current_max)

#define PCB_LS_VOLTAGE_MAX      DT_PROP(DT_PATH(pcb), ls_voltage_max)
#define PCB_MOSFETS_TJ_MAX      DT_PROP(DT_PATH(pcb), mosfets_tj_max)
#define PCB_MOSFETS_TAU_JA      DT_PROP(DT_PATH(pcb), mosfets_tau_ja)
#define PCB_INTERNAL_TREF_MAX   DT_PROP(DT_PATH(pcb), internal_tref_max)

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

    oc_recovery_delay = CONFIG_LOAD_OC_RECOVERY_DELAY;
    lvd_recovery_delay = CONFIG_LOAD_LVD_RECOVERY_DELAY;

    ov_hysteresis = 0.3;

    enable = true;  // switch on in next control() call if everything is fine
}

// this function is called more often than the state machine
void LoadOutput::control()
{
    if (state == LOAD_STATE_ON) {

        // junction temperature calculation model for overcurrent detection
        junction_temperature = junction_temperature +
            (dev_stat.internal_temp - junction_temperature + current * current /
            (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) * (PCB_MOSFETS_TJ_MAX - PCB_INTERNAL_TREF_MAX))
            / (PCB_MOSFETS_TAU_JA * CONFIG_CONTROL_FREQUENCY);

        if (junction_temperature > PCB_MOSFETS_TJ_MAX ||
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
        if (bus->voltage > bus->series_voltage(overvoltage) ||
            bus->voltage > PCB_LS_VOLTAGE_MAX)
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
            bus->voltage < (bus->series_voltage(overvoltage) - ov_hysteresis) &&
            bus->voltage < (PCB_LS_VOLTAGE_MAX - ov_hysteresis))
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
#if LED_EXISTS(load)
        leds_flicker(LED_POS(load), LED_TIMEOUT_INFINITE);
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

extern LoadOutput load;

void load_short_circuit_stop()
{
    load.stop(ERR_LOAD_SHORT_CIRCUIT);
}

#endif /* DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load)) */
