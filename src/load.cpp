/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#include "pcb.h"
#include "config.h"
#include "hardware.h"
#include "leds.h"
#include "device_status.h"
#include "debug.h"
#include "helper.h"

extern DeviceStatus dev_stat;

LoadOutput::LoadOutput(PowerPort *pwr_port) :
    port(pwr_port)
{
    enable = true;
    usb_enable = true;
    state = LOAD_STATE_DISABLED;
    usb_state = LOAD_STATE_DISABLED;

#ifdef __ZEPHYR__
    get_bindings();
#endif

    switch_set(false);
    usb_set(false);
    junction_temperature = 25;              // starting point: 25°C
    oc_recovery_delay = 5*60;               // default: 5 minutes
    lvd_recovery_delay = 60*60;             // default: 1 hour
    ov_hysteresis = 0.3;

    // analog comparator to detect short circuits and trigger immediate load switch-off
#ifdef __MBED__
    // for Zephyr some interrupt conflicts have still to be resolved
    short_circuit_comp_init();
#endif
}

void LoadOutput::usb_state_machine()
{
    // We don't have any overcurrent detection mechanism for USB and the buck converter IC should
    // reduce output power itself in case of failure. Battery overvoltage should also be handled by
    // the DC/DC up to a certain degree.
    // So, we don't use load failure states for the USB output. Only low SOC and exceeded
    // temperature limits force the USB output to be turned off.
    switch (usb_state) {
        case LOAD_STATE_DISABLED:
            if (usb_enable == true) {
                if (port->pos_current_limit > 0) {
                    usb_set(true);
                    usb_state = LOAD_STATE_ON;
                }
                else if (state == LOAD_STATE_OFF_LOW_SOC || state == LOAD_STATE_OFF_TEMPERATURE) {
                    usb_state = state;
                }
            }
            break;
        case LOAD_STATE_ON:
            // currently still same cut-off SOC limit as the load
            if (state == LOAD_STATE_OFF_LOW_SOC || state == LOAD_STATE_OFF_TEMPERATURE) {
                usb_set(false);
                usb_state = state;
            }
            else if (usb_enable == false) {
                usb_set(false);
                usb_state = LOAD_STATE_DISABLED;
            }
            break;
        case LOAD_STATE_OFF_LOW_SOC:
        case LOAD_STATE_OFF_TEMPERATURE:
            if (state == LOAD_STATE_ON) {
                // also go back to normal operation with the USB charging port
                usb_set(true);
                usb_state = LOAD_STATE_ON;
            }
            else if (state == LOAD_STATE_DISABLED) {
                usb_state = LOAD_STATE_DISABLED;
            }
            break;
    }
}

// deprecated function for legacy load state output
int LoadOutput::determine_load_state()
{
    if (pgood) {
        return LOAD_STATE_ON;
    }
    else if (dev_stat.has_error(ERR_LOAD_LOW_SOC)) {
        return LOAD_STATE_OFF_LOW_SOC;
    }
    else if (dev_stat.has_error(ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP)) {
        return LOAD_STATE_OFF_OVERCURRENT;
    }
    else if (dev_stat.has_error(ERR_LOAD_OVERVOLTAGE)) {
        return LOAD_STATE_OFF_OVERVOLTAGE;
    }
    else if (dev_stat.has_error(ERR_LOAD_SHORT_CIRCUIT)) {
        return LOAD_STATE_OFF_SHORT_CIRCUIT;
    }
    else if (enable == false) {
        return LOAD_STATE_DISABLED;
    }
    else {
        return LOAD_STATE_OFF_TEMPERATURE;
    }
}

// this function is called more often than the state machine
void LoadOutput::control()
{
    if (pgood) {

        // junction temperature calculation model for overcurrent detection
        junction_temperature = junction_temperature + (
                dev_stat.internal_temp - junction_temperature +
                port->current * port->current /
                (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) *
                (MOSFET_MAX_JUNCTION_TEMP - INTERNAL_MAX_REFERENCE_TEMP)
            ) / (MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY);

        if (junction_temperature > MOSFET_MAX_JUNCTION_TEMP
            || port->current > LOAD_CURRENT_MAX * 2)
        {
            dev_stat.set_error(ERR_LOAD_OVERCURRENT);
            oc_timestamp = uptime();
        }

        if (dev_stat.has_error(ERR_BAT_UNDERVOLTAGE)) {
            dev_stat.set_error(ERR_LOAD_LOW_SOC);
            lvd_timestamp = uptime();
        }

        // long-term overvoltage (overvoltage transients are detected as an ADC alert and switch
        // off the solar input instead of the load output)
        static int debounce_counter = 0;
        if (port->voltage > port->sink_voltage_max ||
            port->voltage > LOW_SIDE_VOLTAGE_MAX)
        {
            debounce_counter++;
            if (debounce_counter >= CONTROL_FREQUENCY) {      // waited 1s before setting the flag
                dev_stat.set_error(ERR_LOAD_OVERVOLTAGE);
            }
        }
        else {
            debounce_counter = 0;
        }

        if (dev_stat.has_error(ERR_LOAD_ANY)) {
            stop();
        }

        if (enable == false) {
            switch_set(false);
        }
    }
    else {
        // load is off: check if errors are resolved and if load can be switched on

        if (dev_stat.has_error(ERR_LOAD_LOW_SOC) &&
            !dev_stat.has_error(ERR_BAT_UNDERVOLTAGE) &&
            uptime() - lvd_timestamp > lvd_recovery_delay)
        {
            dev_stat.clear_error(ERR_LOAD_LOW_SOC);
        }

        if (dev_stat.has_error(ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP) &&
            uptime() - oc_timestamp > oc_recovery_delay)
        {
            dev_stat.clear_error(ERR_LOAD_OVERCURRENT);
            dev_stat.clear_error(ERR_LOAD_VOLTAGE_DIP);
        }

        if (dev_stat.has_error(ERR_LOAD_OVERVOLTAGE) &&
            port->voltage < (port->sink_voltage_max - ov_hysteresis) &&
            port->voltage < (LOW_SIDE_VOLTAGE_MAX - ov_hysteresis))
        {
            dev_stat.clear_error(ERR_LOAD_OVERVOLTAGE);
        }

        if (dev_stat.has_error(ERR_LOAD_SHORT_CIRCUIT) && enable == false) {
            // stay here until the charge controller is reset or load is manually switched off
            dev_stat.clear_error(ERR_LOAD_SHORT_CIRCUIT);
        }

        // finally switch on if all errors were resolved
        if (enable == true && !dev_stat.has_error(ERR_LOAD_ANY) && port->pos_current_limit > 0) {
            switch_set(true);
        }
    }

    // deprecated load state variable, will be removed in the future
    state = (int)determine_load_state();

    usb_state_machine();
}

void LoadOutput::stop(uint32_t error_flag)
{
    switch_set(false);
    dev_stat.set_error(error_flag);

    // flicker the load LED if failure was most probably caused by the user
    if (error_flag & (ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP | ERR_LOAD_SHORT_CIRCUIT)) {
#ifdef LED_LOAD
        leds_flicker(LED_LOAD);
#endif
        oc_timestamp = uptime();
    }
}
