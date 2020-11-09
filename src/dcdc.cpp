/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dcdc.h"

#include <zephyr.h>

#ifndef UNIT_TEST
#include <drivers/gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(dcdc, CONFIG_LOG_DEFAULT_LEVEL);
#endif

#include <math.h>       // for fabs function
#include <stdlib.h>     // for min/max function
#include <stdio.h>

#include "device_status.h"
#include "helper.h"
#include "half_bridge.h"
#include "eeprom.h"

extern DeviceStatus dev_stat;

#if DT_NODE_EXISTS(DT_PATH(dcdc))

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_ext_sense))
#define HV_EXT_SENSE_GPIO DT_CHILD(DT_PATH(outputs), hv_ext_sense)
#endif

Dcdc::Dcdc(DcBus *high, DcBus *low, DcdcOperationMode op_mode)
{
    hvb = high;
    lvb = low;
    mode           = op_mode;
    enable         = true;
    state          = DCDC_CONTROL_OFF;
    inductor_current_max = DT_PROP(DT_PATH(dcdc), current_max);
    hs_voltage_max = DT_PROP(DT_PATH(pcb), hs_voltage_max);
    ls_voltage_max = DT_PROP(DT_PATH(pcb), ls_voltage_max);
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
    DcBus *in;
    DcBus *out;
    float out_power;

    if (mode == DCDC_MODE_BUCK || (mode == DCDC_MODE_AUTO && inductor_current > 0.1)) {
        // buck mode
        pwr_inc_pwm_direction = 1;
        in = hvb;
        out = lvb;
        out_power = power;
    }
    else {
        // boost mode
        pwr_inc_pwm_direction = -1;
        in = lvb;
        out = hvb;
        out_power = -power;
    }

    if (out_power >= output_power_min) {
        power_good_timestamp = uptime();     // reset the time
    }

    int pwr_inc_goal = 0;     // stores if we want to increase (+1) or decrease (-1) power

    if ((uptime() - power_good_timestamp > 10 || out_power < -1.0) && mode != DCDC_MODE_AUTO) {
        // switch off after 10s low power or negative power (if not in nanogrid mode)
        pwr_inc_goal = 0;
    }
    else if (out->voltage > out->sink_control_voltage()) {
        // output voltage target reached
        state = (out == lvb) ? DCDC_CONTROL_CV_LS : DCDC_CONTROL_CV_HS;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->sink_current_margin < 0) {
        // output charge current limit reached
        state = (out == lvb) ? DCDC_CONTROL_CC_LS : DCDC_CONTROL_CC_HS;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (in->src_current_margin > 0) {
        // input current (negative signs) limit exceeded
        state = (in == hvb) ? DCDC_CONTROL_CC_HS : DCDC_CONTROL_CC_LS;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (fabs(inductor_current) > inductor_current_max) {
        // current above hardware maximum
        state = DCDC_CONTROL_CC_LS;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (in->voltage < in->src_control_voltage() && out_power > output_power_min) {
        // input voltage below limit
        state = (in == hvb) ? DCDC_CONTROL_CV_HS : DCDC_CONTROL_CV_LS;
        pwr_inc_goal = -1;
    }
    else if (temp_mosfets > 80) {
        // temperature limits exceeded
        state = DCDC_CONTROL_DERATING;
        pwr_inc_goal = -1;
    }
    else if (out_power < output_power_min && out->voltage < out->src_control_voltage()) {
        // no load condition (e.g. start-up of nanogrid) --> raise voltage
        pwr_inc_goal = 1;   // increase output power
    }
    else {
        // start MPPT
        state = DCDC_CONTROL_MPPT;
        if (power_prev > out_power) {
            pwm_delta = -pwm_delta;
        }
        pwr_inc_goal = pwm_delta;
    }

#if CONFIG_LOG_DEFAULT_LEVEL == LOG_LEVEL_DBG
    // workaround as LOG_DBG does not support float printing
    printf("P: %.2fW (prev %.2fW), in: %.2fV %.2fA (limit %.2fA), "
        "out: %.2fV (target %.2fV) %.2fA (limit %.2fA), "
        "PWM: %.1f, chg_state: %d, pwr_inc: %d, buck/boost: %d\n",
        out_power, power_prev, in->voltage, in->current, in->neg_current_limit,
        out->voltage, out->sink_voltage_intercept, out->current, out->pos_current_limit,
        half_bridge_get_duty_cycle() * 100.0, state, pwr_inc_goal, pwr_inc_pwm_direction);
#endif

    power_prev = out_power;

    // change duty cycle by single minimum step
    half_bridge_set_ccr(half_bridge_get_ccr() + pwr_inc_goal * pwr_inc_pwm_direction);

    return (pwr_inc_goal == 0);
}

__weak DcdcOperationMode Dcdc::check_start_conditions()
{
    if (enable == false ||
        hvb->voltage > hs_voltage_max ||   // also critical for buck mode because of ringing
        lvb->voltage > ls_voltage_max ||
        lvb->voltage < ls_voltage_min ||
        dev_stat.has_error(ERR_BAT_UNDERVOLTAGE | ERR_BAT_OVERVOLTAGE) ||
        (int)uptime() < (off_timestamp + (int)restart_interval))
    {
        return DCDC_MODE_OFF;
    }

    if (lvb->sink_current_margin > 0 &&
        lvb->voltage < lvb->sink_control_voltage() &&
        hvb->src_current_margin < 0 &&
        hvb->voltage > hvb->src_control_voltage() &&
        hvb->voltage * 0.85 > lvb->voltage)
    {
        return DCDC_MODE_BUCK;
    }

    if (hvb->sink_current_margin > 0 &&
        hvb->voltage < hvb->sink_control_voltage() &&
        lvb->src_current_margin < 0 &&
        lvb->voltage > lvb->src_control_voltage())
    {
        return DCDC_MODE_BOOST;
    }

    return DCDC_MODE_OFF;
}

bool Dcdc::check_hs_mosfet_short()
{
    static uint32_t first_time_detected = 0;
    if (inductor_current > 0.5 && half_bridge_enabled() == false) {
        // if there is current even though the DC/DC is switched off, the
        // high-side MOSFET must be broken --> set flag and let main() decide
        // what to do... (e.g. call dcdc_self_destruction)

        uint32_t now = uptime();
        if (first_time_detected == 0) {
            first_time_detected = now;
        }
        else if (now - first_time_detected > 2) {
            // waited >1s before setting the flag
            dev_stat.set_error(ERR_DCDC_HS_MOSFET_SHORT);
        }
    }

    return dev_stat.has_error(ERR_DCDC_HS_MOSFET_SHORT);
}

bool Dcdc::startup_inhibit(bool reset)
{
    // wait at least 100 ms for voltages to settle
    static const int num_wait_calls =
        (CONFIG_CONTROL_FREQUENCY / 10 >= 1) ? (CONFIG_CONTROL_FREQUENCY / 10) : 1;

    static int startup_delay_counter = 0;

    if (reset) {
        startup_delay_counter = 0;
    }
    else {
        startup_delay_counter++;
    }

    return (startup_delay_counter < num_wait_calls);
}

__weak void Dcdc::control()
{
    if (half_bridge_enabled() == false) {

        if (check_hs_mosfet_short()) {
            return;
        }

        int startup_mode = check_start_conditions();

        if (startup_mode != DCDC_MODE_OFF) {

            // startup allowed, but we need to wait until voltages settle
            if (startup_inhibit()) {
                return;
            }

            const char *mode_name = NULL;
            if (startup_mode == DCDC_MODE_BUCK) {
                mode_name = "buck";
                // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush
                // currents and stress on MOSFETs
                half_bridge_set_duty_cycle(lvb->voltage / (hvb->voltage - 1));
            }
            else {
                mode_name = "boost";
                // Will automatically start with max. duty (0.97) if connected to a
                // nanogrid not yet started up (zero voltage)
                half_bridge_set_duty_cycle(lvb->voltage / (hvb->voltage + 1));
            }

            half_bridge_start();
            power_good_timestamp = uptime();
            printf("DC/DC %s mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", mode_name,
                hvb->voltage, lvb->voltage, half_bridge_get_duty_cycle() * 100);

        }
        else {
            startup_inhibit(true);      // reset inhibit counter
        }
    }
    else { // half bridge is on

        const char *stop_reason = NULL;
        if (lvb->voltage > ls_voltage_max || hvb->voltage > hs_voltage_max) {
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
            stop();
            printf("DC/DC Stop: %s.\n", stop_reason);
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
            if (check_start_conditions() != DCDC_MODE_OFF) {
                half_bridge_set_duty_cycle(lvb->voltage / hvb->voltage);
                half_bridge_start();
                printf("DC/DC test mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n",
                    hvb->voltage, lvb->voltage, half_bridge_get_duty_cycle() * 100);
            }
        }
        else {
            startup_delay_counter++;
        }
    }
}

void Dcdc::stop()
{
    half_bridge_stop();
    state = DCDC_CONTROL_OFF;
    off_timestamp = uptime();
}

void Dcdc::fuse_destruction()
{
    static int counter = 0;

    if (counter > 20) {     // wait 20s to be able to send out data
        LOG_ERR("Charge controller fuse destruction called!\n");
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
    const struct device *dev_hv_ext = device_get_binding(DT_GPIO_LABEL(HV_EXT_SENSE_GPIO, gpios));
    gpio_pin_configure(dev_hv_ext, DT_GPIO_PIN(HV_EXT_SENSE_GPIO, gpios),
        DT_GPIO_FLAGS(HV_EXT_SENSE_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);
#endif
}

void Dcdc::output_hvs_disable()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_ext_sense))
    const struct device *dev_hv_ext = device_get_binding(DT_GPIO_LABEL(HV_EXT_SENSE_GPIO, gpios));
    gpio_pin_configure(dev_hv_ext, DT_GPIO_PIN(HV_EXT_SENSE_GPIO, gpios),
        DT_GPIO_FLAGS(HV_EXT_SENSE_GPIO, gpios) | GPIO_OUTPUT_INACTIVE);
#endif
}

#else

Dcdc::Dcdc(DcBus *high, DcBus *low, DcdcOperationMode op_mode) {}

#endif /* DT_NODE_EXISTS(DT_PATH(dcdc)) */
