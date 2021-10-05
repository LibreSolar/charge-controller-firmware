/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dcdc.h"

#include <zephyr.h>

#include <device.h>
#include <drivers/gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(dcdc, CONFIG_DCDC_LOG_LEVEL);

#include <math.h>       // for fabs function
#include <stdlib.h>     // for min/max function
#include <stdio.h>

#include "device_status.h"
#include "helper.h"
#include "half_bridge.h"
#include "data_storage.h"
#include "setup.h"

extern DeviceStatus dev_stat;

#if BOARD_HAS_DCDC

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_out))
#define HV_OUT_NODE DT_CHILD(DT_PATH(outputs), hv_out)
#endif

Dcdc::Dcdc(DcBus *high, DcBus *low, DcdcOperationMode op_mode)
{
    hvb = high;
    lvb = low;
    mode           = op_mode;
    enable         = true;
    state          = DCDC_CONTROL_OFF;
    inductor_current_max = DT_PROP(DT_PATH(pcb), dcdc_current_max);
    hs_voltage_max = DT_PROP(DT_PATH(pcb), hs_voltage_max);
    ls_voltage_max = DT_PROP(DT_PATH(pcb), ls_voltage_max);
    ls_voltage_min = 9.0;
    output_power_min = 1;         // switch off if power < 1 W
    restart_interval = 60;
    off_timestamp = -10000;       // start immediately
    pwm_delta = 1;                // start-condition of duty cycle pwr_inc_pwm_direction

    // lower duty limit might have to be adjusted dynamically depending on LS voltage
    half_bridge_init(DT_PROP(DT_INST(0, half_bridge), frequency) / 1000,
        DT_PROP(DT_INST(0, half_bridge), deadtime), 12 / hs_voltage_max, 0.97);
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

    if ((uptime() - power_good_timestamp > 10 || out_power < -10.0) && mode != DCDC_MODE_AUTO) {
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

#if CONFIG_DCDC_LOG_LEVEL == LOG_LEVEL_DBG
    // workaround as LOG_DBG does not support float printing
    printf("P: %.2fW (prev %.2fW), ind. current: %.2f, "
        "in: %.2fV, %.2fA margin, out: %.2fV (target %.2fV), %.2fA margin, "
        "PWM: %.1f, chg_state: %d, pwr_inc: %d, buck/boost: %d\n",
        out_power, power_prev, inductor_current, in->voltage, in->src_current_margin,
        out->voltage, out->sink_voltage_intercept, out->sink_current_margin,
        half_bridge_get_duty_cycle() * 100.0, state, pwr_inc_goal, pwr_inc_pwm_direction);
#endif

    power_prev = out_power;

#ifdef CONFIG_SOC_SERIES_STM32G4X
    // reduced step size for fast microcontroller
    half_bridge_set_ccr(half_bridge_get_ccr() + pwr_inc_goal * pwr_inc_pwm_direction * 3);
#else
    // change duty cycle by single minimum step
    half_bridge_set_ccr(half_bridge_get_ccr() + pwr_inc_goal * pwr_inc_pwm_direction);
#endif

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
    static uint32_t inhibit_start;

    if (reset) {
        inhibit_start = uptime();
        return true;
    }
    else {
        return (uptime() < inhibit_start + DCDC_STARTUP_INHIBIT_TIME);
    }
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
        const char *stop_reason = NULL;
        if (lvb->voltage > ls_voltage_max || hvb->voltage > hs_voltage_max) {
            stop_reason = "emergency (voltage limits exceeded)";
        }
        else if (enable == false) {
            stop_reason = "disabled";
        }
        else {
            if (half_bridge_get_duty_cycle() > 0.50) {
                half_bridge_set_ccr(half_bridge_get_ccr() - 1);
            }
            else {
                half_bridge_set_ccr(half_bridge_get_ccr() + 1);
            }
        }
        if (stop_reason != NULL) {
            stop();
            printf("DC/DC Stop: %s.\n", stop_reason);
        }
    }
    else {
        if (check_start_conditions() != DCDC_MODE_OFF) {
            // startup allowed, but we need to wait until voltages settle
            if (startup_inhibit()) {
                return;
            }

            half_bridge_set_duty_cycle(lvb->voltage / hvb->voltage);
            half_bridge_start();
            printf("DC/DC test mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n",
                hvb->voltage, lvb->voltage, half_bridge_get_duty_cycle() * 100);
        }
        else {
            startup_inhibit(true);      // reset inhibit counter
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
        data_storage_write();
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
#ifdef HV_OUT_NODE
    const struct device *hv_out = DEVICE_DT_GET(DT_GPIO_CTLR(HV_OUT_NODE, gpios));
    if (device_is_ready(hv_out)) {
        gpio_pin_configure(hv_out, DT_GPIO_PIN(HV_OUT_NODE, gpios),
            DT_GPIO_FLAGS(HV_OUT_NODE, gpios) | GPIO_OUTPUT_ACTIVE);
    }
#endif
}

void Dcdc::output_hvs_disable()
{
#ifdef HV_OUT_NODE
    const struct device *hv_out = DEVICE_DT_GET(DT_GPIO_CTLR(HV_OUT_NODE, gpios));
    if (device_is_ready(hv_out)) {
        gpio_pin_configure(hv_out, DT_GPIO_PIN(HV_OUT_NODE, gpios),
            DT_GPIO_FLAGS(HV_OUT_NODE, gpios) | GPIO_OUTPUT_INACTIVE);
    }
#endif
}

#else

Dcdc::Dcdc(DcBus *high, DcBus *low, DcdcOperationMode op_mode) {}

#endif /* BOARD_HAS_DCDC */
