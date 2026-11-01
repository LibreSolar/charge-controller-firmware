/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dcdc.h"

#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include <math.h> // for fabs function
#include <stdio.h>
#include <stdlib.h> // for min/max function

#include "board.h"
#include "device_status.h"
#include "helper.h"

#include <libresolar/drivers/half_bridge.h>

#include <thingset.h>
#include <thingset/sdk.h>
#include <thingset/storage.h>

LOG_MODULE_REGISTER(dcdc, CONFIG_DCDC_LOG_LEVEL);

#if BOARD_HAS_DCDC

#define BUCK_DUTY_POWER_DECREASE (-1)
#define BUCK_DUTY_POWER_INCREASE (1)

#define BOOST_DUTY_POWER_DECREASE (1)
#define BOOST_DUTY_POWER_INCREASE (-1)

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), hv_out))
#define HV_OUT_NODE DT_CHILD(DT_PATH(outputs), hv_out)
#endif

static void output_hvs_enable(void);
static void output_hvs_disable(void);

void dcdc_init(struct dcdc *dcdc, const struct device *hb, struct dc_bus *high, struct dc_bus *low,
               enum dcdc_operation_mode op_mode)
{
    dcdc->half_bridge = hb;
    dcdc->hvb = high;
    dcdc->lvb = low;
    dcdc->mode = op_mode;
    dcdc->enable = true;
    dcdc->state = DCDC_CONTROL_OFF;
    dcdc->inductor_current_max = DT_PROP(DT_PATH(pcb), dcdc_current_max);
    dcdc->hs_voltage_max = DT_PROP(DT_PATH(pcb), hs_voltage_max);
    dcdc->ls_voltage_max = DT_PROP(DT_PATH(pcb), ls_voltage_max);
    dcdc->ls_voltage_min = 9.0;
    dcdc->output_power_min = 1; // switch off if power < 1 W
    dcdc->restart_interval = 60;
    dcdc->off_timestamp = -10000; // start immediately

    if (!device_is_ready(dcdc->half_bridge)) {
        LOG_ERR("Half bridge not ready");
        dcdc->enable = false;
        return;
    }

    // lower duty limit might have to be adjusted dynamically depending on LS voltage
    half_bridge_set_duty_limits(dcdc->half_bridge, 12 / dcdc->hs_voltage_max, 0.97);
    half_bridge_set_duty_cycle(dcdc->half_bridge, 0.97); // init with allowed value
}

static void perturb_observe_buck(struct dcdc *dcdc)
{
    if (dcdc->power >= dcdc->output_power_min) {
        dcdc->power_good_timestamp = uptime();
    }

    if ((uptime() - dcdc->power_good_timestamp > 10 || dcdc->power < -10.0f)
        && dcdc->mode != DCDC_MODE_AUTO)
    {
        // switch off after 10s low power or negative power (if not in nanogrid mode)
        dcdc->pwm_direction = 0;
    }
    else if (dcdc->lvb->voltage > dc_bus_sink_control_voltage(dcdc->lvb, 0)) {
        dcdc->state = DCDC_CONTROL_CV_LS;
        dcdc->pwm_direction = BUCK_DUTY_POWER_DECREASE;
    }
    else if (dcdc->lvb->sink_current_margin < 0
             || dcdc->inductor_current > dcdc->inductor_current_max)
    {
        dcdc->state = DCDC_CONTROL_CC_LS;
        dcdc->pwm_direction = BUCK_DUTY_POWER_DECREASE;
    }
    else if (dcdc->hvb->src_current_margin > 0) {
        dcdc->state = DCDC_CONTROL_CC_HS;
        dcdc->pwm_direction = BUCK_DUTY_POWER_DECREASE;
    }
    else if (dcdc->hvb->voltage < dc_bus_src_control_voltage(dcdc->hvb, 0)
             && dcdc->power > dcdc->output_power_min)
    {
        // input voltage below limit
        dcdc->state = DCDC_CONTROL_CV_HS;
        dcdc->pwm_direction = BUCK_DUTY_POWER_DECREASE;
    }
    else if (dcdc->temp_mosfets > DCDC_MOSFETS_MAX_TEMP) {
        dcdc->state = DCDC_CONTROL_DERATING;
        dcdc->pwm_direction = BUCK_DUTY_POWER_DECREASE;
    }
    else if (dcdc->power < dcdc->output_power_min
             && dcdc->lvb->voltage < dc_bus_src_control_voltage(dcdc->lvb, 0))
    {
        // no load condition (e.g. start-up of nanogrid) --> raise voltage
        dcdc->pwm_direction = BUCK_DUTY_POWER_INCREASE;
    }
    else {
        dcdc->state = DCDC_CONTROL_MPPT;
        if (dcdc->power_prev > dcdc->power) {
            dcdc->pwm_direction = -dcdc->pwm_direction;
        }
    }

    dcdc->power_prev = dcdc->power;
}

static void perturb_observe_boost(struct dcdc *dcdc)
{
    if (-dcdc->power >= dcdc->output_power_min) {
        dcdc->power_good_timestamp = uptime();
    }

    if ((uptime() - dcdc->power_good_timestamp > 10 || -dcdc->power < -10.0f)
        && dcdc->mode != DCDC_MODE_AUTO)
    {
        // switch off after 10s low power or negative power (if not in nanogrid mode)
        dcdc->pwm_direction = 0;
    }
    else if (dcdc->hvb->voltage > dc_bus_sink_control_voltage(dcdc->hvb, 0)) {
        dcdc->state = DCDC_CONTROL_CV_HS;
        dcdc->pwm_direction = BOOST_DUTY_POWER_DECREASE;
    }
    else if (dcdc->hvb->sink_current_margin < 0) {
        dcdc->state = DCDC_CONTROL_CC_HS;
        dcdc->pwm_direction = BOOST_DUTY_POWER_DECREASE;
    }
    else if (dcdc->lvb->src_current_margin > 0
             || -dcdc->inductor_current > dcdc->inductor_current_max)
    {
        dcdc->state = DCDC_CONTROL_CC_LS;
        dcdc->pwm_direction = BOOST_DUTY_POWER_DECREASE;
    }
    else if (dcdc->lvb->voltage < dc_bus_src_control_voltage(dcdc->lvb, 0)
             && -dcdc->power > dcdc->output_power_min)
    {
        // input voltage below limit
        dcdc->state = DCDC_CONTROL_CV_LS;
        dcdc->pwm_direction = BOOST_DUTY_POWER_DECREASE;
    }
    else if (dcdc->temp_mosfets > DCDC_MOSFETS_MAX_TEMP) {
        dcdc->state = DCDC_CONTROL_DERATING;
        dcdc->pwm_direction = BOOST_DUTY_POWER_DECREASE;
    }
    else if (-dcdc->power < dcdc->output_power_min
             && dcdc->hvb->voltage < dc_bus_src_control_voltage(dcdc->hvb, 0))
    {
        // no load condition (e.g. start-up of nanogrid) --> raise voltage
        dcdc->pwm_direction = BOOST_DUTY_POWER_INCREASE;
    }
    else {
        dcdc->state = DCDC_CONTROL_MPPT;
        if (-dcdc->power_prev > -dcdc->power) {
            dcdc->pwm_direction = -dcdc->pwm_direction;
        }
    }

    dcdc->power_prev = dcdc->power;
}

__weak enum dcdc_operation_mode dcdc_check_start_conditions(struct dcdc *dcdc)
{
    if (dcdc->enable == false
        || dcdc->hvb->voltage
               > dcdc->hs_voltage_max // also critical for buck mode because of ringing
        || dcdc->lvb->voltage > dcdc->ls_voltage_max || dcdc->lvb->voltage < dcdc->ls_voltage_min
        || device_status_has_error(&dev_stat, ERR_BAT_UNDERVOLTAGE | ERR_BAT_OVERVOLTAGE)
        || (int)uptime() < (dcdc->off_timestamp + (int)dcdc->restart_interval))
    {
        return DCDC_MODE_OFF;
    }

    if (dcdc->lvb->sink_current_margin > 0
        && dcdc->lvb->voltage < dc_bus_sink_control_voltage(dcdc->lvb, 0)
        && dcdc->hvb->src_current_margin < 0
        && dcdc->hvb->voltage > dc_bus_src_control_voltage(dcdc->hvb, 0)
        && dcdc->hvb->voltage * 0.85f > dcdc->lvb->voltage)
    {
        return DCDC_MODE_BUCK;
    }

    if (dcdc->hvb->sink_current_margin > 0
        && dcdc->hvb->voltage < dc_bus_sink_control_voltage(dcdc->hvb, 0)
        && dcdc->lvb->src_current_margin < 0
        && dcdc->lvb->voltage > dc_bus_src_control_voltage(dcdc->lvb, 0))
    {
        return DCDC_MODE_BOOST;
    }

    return DCDC_MODE_OFF;
}

static bool check_hs_mosfet_short(struct dcdc *dcdc)
{
    static uint32_t first_time_detected;
    if (half_bridge_is_enabled(dcdc->half_bridge) == false && dcdc->inductor_current > 0.5F
        && dcdc->lvb->voltage_filtered > dc_bus_sink_control_voltage(dcdc->lvb, 0))
    {
        // if there is current even though the DC/DC is switched off, the
        // high-side MOSFET must be broken --> set flag and let main() decide
        // what to do... (e.g. call dcdc_self_destruction)

        uint32_t now = uptime();
        if (first_time_detected == 0) {
            first_time_detected = now;
        }
        else if (now - first_time_detected > 10) {
            // waited approx 10s before setting the flag
            device_status_set_error(&dev_stat, ERR_DCDC_HS_MOSFET_SHORT);
        }
    }
    else {
        first_time_detected = 0;
    }

    return device_status_has_error(&dev_stat, ERR_DCDC_HS_MOSFET_SHORT);
}

static bool startup_inhibit(bool reset)
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

__weak void dcdc_control(struct dcdc *dcdc)
{
    if (device_status_has_error(&dev_stat, ERR_DCDC_HS_MOSFET_SHORT)) {
        // DC/DC must not be operated anymore and the low-side switch might be forced on to blow
        // the fuse, so don't touch the half bridge
        return;
    }

    if (half_bridge_is_enabled(dcdc->half_bridge) == false) {

        if (check_hs_mosfet_short(dcdc)) {
            return;
        }

        int startup_mode = dcdc_check_start_conditions(dcdc);

        if ((startup_mode == DCDC_MODE_BUCK && dcdc->mode == DCDC_MODE_BUCK)
            || (startup_mode == DCDC_MODE_BOOST && dcdc->mode == DCDC_MODE_BOOST)
            || (startup_mode != DCDC_MODE_OFF && dcdc->mode == DCDC_MODE_AUTO))
        {
            output_hvs_enable();

            // startup allowed, but we need to wait until voltages settle
            if (startup_inhibit(false)) {
                return;
            }

            const char *mode_name = NULL;
            if (startup_mode == DCDC_MODE_BUCK) {
                mode_name = "buck";
                dcdc->pwm_direction = BUCK_DUTY_POWER_INCREASE;
                // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush
                // currents and stress on MOSFETs
                half_bridge_set_duty_cycle(dcdc->half_bridge,
                                           dcdc->lvb->voltage / (dcdc->hvb->voltage - 1));
            }
            else {
                mode_name = "boost";
                dcdc->pwm_direction = BOOST_DUTY_POWER_INCREASE;
                // Will automatically start with max. duty (0.97) if connected to a
                // nanogrid not yet started up (zero voltage)
                half_bridge_set_duty_cycle(dcdc->half_bridge,
                                           dcdc->lvb->voltage / (dcdc->hvb->voltage + 1));
            }

            half_bridge_start(dcdc->half_bridge);
            dcdc->power_good_timestamp = uptime();
            printf("DC/DC %s mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", mode_name,
                   (double)dcdc->hvb->voltage, (double)dcdc->lvb->voltage,
                   (double)half_bridge_get_duty_cycle(dcdc->half_bridge) * 100.0);
        }
        else {
            startup_inhibit(true); // reset inhibit counter
        }
    }
    else { // half bridge is on

        const char *stop_reason = NULL;
        if (dcdc->lvb->voltage > dcdc->ls_voltage_max || dcdc->hvb->voltage > dcdc->hs_voltage_max)
        {
            stop_reason = "emergency (voltage limits exceeded)";
        }
        else if (dcdc->enable == false) {
            stop_reason = "disabled";
        }
        else {
            if (dcdc->mode == DCDC_MODE_BUCK
                || (dcdc->mode == DCDC_MODE_AUTO && dcdc->inductor_current > 0.1f))
            {
                perturb_observe_buck(dcdc);
            }
            else {
                perturb_observe_boost(dcdc);
            }

            if (dcdc->pwm_direction != 0) {
                half_bridge_set_compare(dcdc->half_bridge,
                                        half_bridge_get_compare(dcdc->half_bridge)
                                            + dcdc->pwm_direction * CONFIG_DCDC_DUTY_STEP_SIZE);

                // requires floating point support with CONFIG_CBPRINTF_FP_SUPPORT=y
                LOG_DBG("P %.2fW, inductor %.2fA, HS: %.2fV, %.2fA margin, "
                        "LS: %.2fV (target %.2fV), %.2fA margin, "
                        "PWM: %.1f, dcdc_state: %d, pwm_direction: %d",
                        (double)dcdc->power, (double)dcdc->inductor_current,
                        (double)dcdc->hvb->voltage, (double)dcdc->hvb->src_current_margin,
                        (double)dcdc->lvb->voltage, (double)dcdc->lvb->sink_voltage_intercept,
                        (double)dcdc->lvb->sink_current_margin,
                        (double)half_bridge_get_duty_cycle(dcdc->half_bridge) * 100.0, dcdc->state,
                        dcdc->pwm_direction);
            }
            else {
                stop_reason = "low power";
            }
        }

        if (stop_reason != NULL) {
            dcdc_stop(dcdc);
            printf("DC/DC Stop: %s.\n", stop_reason);
        }
    }
}

void dcdc_test(struct dcdc *dcdc)
{
    if (half_bridge_is_enabled(dcdc->half_bridge)) {
        const char *stop_reason = NULL;
        if (dcdc->lvb->voltage > dcdc->ls_voltage_max || dcdc->hvb->voltage > dcdc->hs_voltage_max)
        {
            stop_reason = "emergency (voltage limits exceeded)";
        }
        else if (dcdc->enable == false) {
            stop_reason = "disabled";
        }
        else {
            if (half_bridge_get_duty_cycle(dcdc->half_bridge) > 0.50f) {
                half_bridge_set_compare(dcdc->half_bridge,
                                        half_bridge_get_compare(dcdc->half_bridge) - 1);
            }
            else {
                half_bridge_set_compare(dcdc->half_bridge,
                                        half_bridge_get_compare(dcdc->half_bridge) + 1);
            }
        }
        if (stop_reason != NULL) {
            dcdc_stop(dcdc);
            printf("DC/DC Stop: %s.\n", stop_reason);
        }
    }
    else {
        if (dcdc_check_start_conditions(dcdc) != DCDC_MODE_OFF) {
            // startup allowed, but we need to wait until voltages settle
            if (startup_inhibit(false)) {
                return;
            }

            half_bridge_set_duty_cycle(dcdc->half_bridge, dcdc->lvb->voltage / dcdc->hvb->voltage);
            half_bridge_start(dcdc->half_bridge);
            printf("DC/DC test mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n",
                   (double)dcdc->hvb->voltage, (double)dcdc->lvb->voltage,
                   (double)half_bridge_get_duty_cycle(dcdc->half_bridge) * 100.0);
        }
        else {
            startup_inhibit(true); // reset inhibit counter
        }
    }
}

void dcdc_stop(struct dcdc *dcdc)
{
    half_bridge_stop(dcdc->half_bridge);
    dcdc->state = DCDC_CONTROL_OFF;
    dcdc->off_timestamp = uptime();
    output_hvs_disable();
}

void dcdc_fuse_destruction(struct dcdc *dcdc)
{
    static int counter = 0;

    if (counter > 20) { // wait 20s to be able to send out data
        if (counter == 21) {
            LOG_ERR("Charge controller fuse destruction called!\n");
#ifdef CONFIG_THINGSET_STORAGE
            thingset_storage_save();
#endif
        }
        // repeated in case the half bridge was stopped in the meantime
        half_bridge_force_low_side(dcdc->half_bridge);
        // now the fuse should be triggered and we disappear
    }
    counter++;
}

static void output_hvs_enable(void)
{
#ifdef HV_OUT_NODE
    const struct device *hv_out = DEVICE_DT_GET(DT_GPIO_CTLR(HV_OUT_NODE, gpios));
    if (device_is_ready(hv_out)) {
        gpio_pin_configure(hv_out, DT_GPIO_PIN(HV_OUT_NODE, gpios),
                           DT_GPIO_FLAGS(HV_OUT_NODE, gpios) | GPIO_OUTPUT_ACTIVE);
    }
#endif
}

static void output_hvs_disable(void)
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

void dcdc_init(struct dcdc *dcdc, const struct device *hb, struct dc_bus *high, struct dc_bus *low,
               enum dcdc_operation_mode op_mode)
{}

#endif /* BOARD_HAS_DCDC */
