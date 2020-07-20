/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dcdc.h"

#include <zephyr.h>

#ifndef UNIT_TEST
#include <drivers/gpio.h>
#endif

#include <math.h>       // for fabs function
#include <stdlib.h>     // for min/max function
#include <stdio.h>

#include "device_status.h"
#include "debug.h"
#include "helper.h"
#include "half_bridge.h"
#include "eeprom.h"

extern DeviceStatus dev_stat;

#if DT_NODE_EXISTS(DT_PATH(dcdc))

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_ext_sense))
#define HV_EXT_SENSE_GPIO DT_CHILD(DT_PATH(outputs), hv_ext_sense)
#endif

Dcdc::Dcdc(PowerPort *hv_side, PowerPort *lv_side, DcdcOperationMode op_mode)
{
    hvs = hv_side;
    lvs = lv_side;
    mode           = op_mode;
    enable         = true;
    state          = DCDC_STATE_OFF;
    lvs->neg_current_limit = -DT_PROP(DT_PATH(dcdc), current_max);
    lvs->pos_current_limit = DT_PROP(DT_PATH(dcdc), current_max);
    ls_current_max = DT_PROP(DT_PATH(dcdc), current_max);
    hs_voltage_max = DT_PROP(DT_PATH(pcb), hs_voltage_max);
    ls_voltage_max = DT_PROP(DT_PATH(pcb), hs_voltage_max);
    ls_voltage_min = 9.0;
    output_power_min = 1;         // switch off if power < 1 W
    restart_interval = 60;
    off_timestamp = -10000;       // start immediately
    pwm_delta = 1;                // start-condition of duty cycle pwr_inc_pwm_direction

    // lower duty limit might have to be adjusted dynamically depending on LS voltage
    half_bridge_init(DT_PROP(DT_PATH(dcdc), pwm_frequency) / 1000,
        DT_PROP(DT_PATH(dcdc), pwm_deadtime), 12 / hs_voltage_max, 0.97);
}

int Dcdc::perturb_observe_controller()
{
    int pwr_inc_pwm_direction;      // direction of PWM duty cycle change for increasing power
    PowerPort *in;
    PowerPort *out;

    if (mode == MODE_MPPT_BUCK ||
        (mode == MODE_NANOGRID && lvs->current > 0.1))
    {
        // buck mode
        pwr_inc_pwm_direction = 1;
        in = hvs;
        out = lvs;
    }
    else {
        // boost mode
        pwr_inc_pwm_direction = -1;
        in = lvs;
        out = hvs;
    }

    if (out->power >= output_power_min) {
        power_good_timestamp = uptime();     // reset the time
    }

    int pwr_inc_goal = 0;     // stores if we want to increase (+1) or decrease (-1) power

    if ((uptime() - power_good_timestamp > 10 || out->power < -1.0) && mode != MODE_NANOGRID) {
        // swith off after 10s low power or negative power (if not in nanogrid mode)
        pwr_inc_goal = 0;
    }
    else if (out->bus->voltage > out->bus->sink_control_voltage()) {
        // output voltage target reached
        state = DCDC_STATE_CV;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->current > out->pos_current_limit) {
        // output charge current limit reached
        state = DCDC_STATE_CC;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (fabs(lvs->current) > ls_current_max    // current above hardware maximum
        || temp_mosfets > 80                        // temperature limits exceeded
        || (in->bus->voltage < in->bus->src_control_voltage()
            && out->current > 0.1)                  // input voltage below limit
        || in->current < in->neg_current_limit)     // input current (negative signs) limit exceeded
    {
        state = DCDC_STATE_DERATING;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->power < output_power_min && out->bus->voltage < out->bus->src_control_voltage()) {
        // no load condition (e.g. start-up of nanogrid) --> raise voltage
        pwr_inc_goal = 1;   // increase output power
    }
    else {
        // start MPPT
        state = DCDC_STATE_MPPT;
        if (power_prev > out->power) {
            pwm_delta = -pwm_delta;
        }
        pwr_inc_goal = pwm_delta;
    }

    print_test("P: %.2fW (prev %.2fW), in: %.2fV %.2fA (margin %.2fA limit %.2fA), "
        "out: %.2fV (target %.2fV) %.2fA (margin %.2fA limit %.2fA), "
        "PWM: %.1f, chg_state: %d, pwr_inc: %d, buck/boost: %d\n",
        out->power, power_prev, in->bus->voltage, in->current, in->bus->src_current_margin,
        in->neg_current_limit,
        out->bus->voltage, out->bus->sink_voltage_bound, out->current, out->bus->sink_current_margin,
        out->pos_current_limit,
        half_bridge_get_duty_cycle() * 100.0, state, pwr_inc_goal, pwr_inc_pwm_direction);

    power_prev = out->power;

    // change duty cycle by single minimum step
    half_bridge_set_ccr(half_bridge_get_ccr() + pwr_inc_goal * pwr_inc_pwm_direction);

    return (pwr_inc_goal == 0);
}

int Dcdc::check_start_conditions()
{
    if (enable == false ||
        hvs->bus->voltage > hs_voltage_max ||   // also critical for buck mode because of ringing
        lvs->bus->voltage > ls_voltage_max ||
        lvs->bus->voltage < ls_voltage_min ||
        dev_stat.has_error(ERR_BAT_UNDERVOLTAGE | ERR_BAT_OVERVOLTAGE) ||
        (int)uptime() < (off_timestamp + (int)restart_interval))
    {
        return 0;       // no energy transfer allowed
    }

    if (lvs->bus->sink_current_margin > 0 &&
        lvs->bus->voltage < lvs->bus->sink_control_voltage() &&
        hvs->bus->src_current_margin < 0 &&
        hvs->bus->voltage > hvs->bus->src_control_voltage() &&
        hvs->bus->voltage * 0.85 > lvs->bus->voltage)
    {
        return 1;       // start in buck mode allowed
    }

    if (hvs->bus->sink_current_margin > 0 &&
        hvs->bus->voltage < hvs->bus->sink_control_voltage() &&
        lvs->bus->src_current_margin < 0 &&
        lvs->bus->voltage > lvs->bus->src_control_voltage())
    {
        return -1;      // start in boost mode allowed
    }

    return 0;
}

__weak void Dcdc::control()
{
    if (half_bridge_enabled()) {
        const char *stop_reason = NULL;
        if (lvs->bus->voltage > ls_voltage_max || hvs->bus->voltage > hs_voltage_max) {
            stop_reason = "emergency (voltage limits exceeded)";
        }
        else if (enable == false) {
            stop_reason = "disabled";
        }
        else {
            int err = perturb_observe_controller();
            if (err != 0) {
                stop_reason =  "low power";
            }
        }

        if (stop_reason != NULL) {
            half_bridge_stop();
            state = DCDC_STATE_OFF;
            off_timestamp = uptime();
            print_info("DC/DC Stop: %s.\n",stop_reason);
        }
    }
    else { // half bridge is off

        static int current_debounce_counter = 0;
        if (lvs->current > 0.5) {
            // if there is current even though the DC/DC is switched off, the
            // high-side MOSFET must be broken --> set flag and let main() decide
            // what to do... (e.g. call dcdc_self_destruction)
            current_debounce_counter++;
            // waited 1s before setting the flag
            if (current_debounce_counter > CONFIG_CONTROL_FREQUENCY) {
                dev_stat.set_error(ERR_DCDC_HS_MOSFET_SHORT);
            }
        }
        else {  // hardware is fine
            current_debounce_counter = 0;

            static int startup_delay_counter = 0;

            // wait at least 100 ms for voltages to settle
            static const int num_wait_calls =
                (CONFIG_CONTROL_FREQUENCY / 10 >= 1) ? (CONFIG_CONTROL_FREQUENCY / 10) : 1;

            int startup_mode = check_start_conditions();

            if (startup_mode == 0) {
                startup_delay_counter = 0; // reset counter
            }
            else {
                startup_delay_counter++;
                if (startup_delay_counter > num_wait_calls) {
                    const char *mode_name = NULL;
                    if (startup_mode == 1) {
                        mode_name = "buck";
                        // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush
                        // currents and stress on MOSFETs
                        half_bridge_set_duty_cycle(lvs->bus->voltage / (hvs->bus->voltage - 1));
                    }
                    else {
                        mode_name = "boost";
                        // Will automatically start with max. duty (0.97) if connected to a
                        // nanogrid not yet started up (zero voltage)
                        half_bridge_set_duty_cycle(lvs->bus->voltage / (hvs->bus->voltage + 1));
                    }
                    half_bridge_start();
                    power_good_timestamp = uptime();
                    print_info("DC/DC %s mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", mode_name,
                        hvs->bus->voltage, lvs->bus->voltage, half_bridge_get_duty_cycle() * 100);
                    startup_delay_counter = 0; // ensure we will have a delay before next start
                }
            }
        }
    }
}

void Dcdc::test()
{
    if (half_bridge_enabled()) {
        if (half_bridge_get_duty_cycle() > 0.5) {
            half_bridge_set_ccr(half_bridge_get_ccr() - 1);
        }
    }
    else {
        static int startup_delay_counter = 0;

        // wait at least 100 ms for voltages to settle
        static const int num_wait_calls =
            (CONFIG_CONTROL_FREQUENCY / 10 >= 1) ? (CONFIG_CONTROL_FREQUENCY / 10) : 1;

        if (startup_delay_counter > num_wait_calls) {
            if (check_start_conditions() != 0) {
                half_bridge_set_duty_cycle(lvs->bus->voltage / hvs->bus->voltage);
                half_bridge_start();
                print_info("DC/DC test mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n",
                    hvs->bus->voltage, lvs->bus->voltage, half_bridge_get_duty_cycle() * 100);
            }
        }
        else {
            startup_delay_counter++;
        }
    }
}

void Dcdc::emergency_stop()
{
    half_bridge_stop();
    state = DCDC_STATE_OFF;
    off_timestamp = uptime();
}

void Dcdc::fuse_destruction()
{
    static int counter = 0;

    if (counter > 20) {     // wait 20s to be able to send out data
        print_error("Charge controller fuse destruction called!\n");
        eeprom_store_data();
        half_bridge_stop();
        half_bridge_init(50, 0, 0, 0.98);   // reset safety limits to allow 0% duty cycle
        half_bridge_set_duty_cycle(0);
        half_bridge_start();
        // now the fuse should be triggered and we disappear
    }
    counter++;
}

void Dcdc::output_hvs_enable()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_ext_sense))
    struct device *dev_hv_ext = device_get_binding(DT_GPIO_LABEL(HV_EXT_SENSE_GPIO, gpios));
    gpio_pin_configure(dev_hv_ext, DT_GPIO_PIN(HV_EXT_SENSE_GPIO, gpios),
        DT_GPIO_FLAGS(HV_EXT_SENSE_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);
#endif
}

void Dcdc::output_hvs_disable()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_ext_sense))
    struct device *dev_hv_ext = device_get_binding(DT_GPIO_LABEL(HV_EXT_SENSE_GPIO, gpios));
    gpio_pin_configure(dev_hv_ext, DT_GPIO_PIN(HV_EXT_SENSE_GPIO, gpios),
        DT_GPIO_FLAGS(HV_EXT_SENSE_GPIO, gpios) | GPIO_OUTPUT_INACTIVE);
#endif
}

#else

Dcdc::Dcdc(PowerPort *hv_side, PowerPort *lv_side, DcdcOperationMode op_mode) {}

#endif /* DT_NODE_EXISTS(DT_PATH(dcdc)) */
