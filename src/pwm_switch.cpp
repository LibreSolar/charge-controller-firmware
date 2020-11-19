/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pwm_switch.h"

#include <zephyr.h>

#ifndef UNIT_TEST
#include <logging/log.h>
LOG_MODULE_REGISTER(pwm_switch, CONFIG_LOG_DEFAULT_LEVEL);
#endif

#include <stdio.h>
#include <time.h>

#include "mcu.h"
#include "daq.h"
#include "helper.h"
#include "setup.h"

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))

#define PWM_CURRENT_MAX (DT_PROP(DT_CHILD(DT_PATH(outputs), pwm_switch), current_max))
#define PWM_PERIOD (DT_PHA(DT_CHILD(DT_PATH(outputs), pwm_switch), pwms, period))

bool PwmSwitch::active()
{
    return pwm_active();
}

bool PwmSwitch::signal_high()
{
    return pwm_signal_high();
}

PwmSwitch::PwmSwitch(DcBus *dc_bus) :
    PowerPort(dc_bus)
{
    // period stored in nanoseconds
    pwm_signal_init_registers(1000 * 1000 * 1000 / PWM_PERIOD);
}

void PwmSwitch::test()
{
    if (pwm_active() && enable == false) {
        pwm_signal_stop();
        off_timestamp = uptime();
        LOG_INF("PWM test mode stop.");
    }
    else if (!pwm_active() && enable == true) {
        // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 50 ms
        adc_upper_alert_inhibit(ADC_POS(v_low), 50);
        pwm_signal_start(0.9F);
        LOG_INF("PWM test mode start.");
    }
}

void PwmSwitch::control()
{
    if (pwm_active()) {
        if (current < -0.1F) {
            power_good_timestamp = uptime();     // reset the time
        }

        if (neg_current_limit == 0
            || (uptime() - power_good_timestamp > 10)      // low power since 10s
            || current > 0.5F         // discharging battery into solar panel --> stop
            || bus->voltage < 9.0F    // not enough voltage for MOSFET drivers anymore
            || enable == false)
        {
            pwm_signal_stop();
            off_timestamp = uptime();
            LOG_INF("PWM charger stop, current = %d mA", (int)(current * 1000.0F));
        }
        else if (bus->voltage > bus->sink_control_voltage() + 0.3F) {
            pwm_signal_stop();
            off_timestamp = uptime();
            dev_stat.set_error(ERR_PWM_SWITCH_OVERVOLTAGE);
            LOG_INF("PWM charger stop, overvoltage.");
        }
        else if (bus->voltage > bus->sink_control_voltage()   // bus voltage above target
            || current < neg_current_limit                    // port current limit exceeded
            || current < -PWM_CURRENT_MAX)                    // PCB current limit exceeded
        {
            // decrease power, as limits were reached

            // the gate driver switch-off time is quite high (fall time around 1ms), so very short
            // on or off periods (duty cycle close to 0 and 1) should be avoided
            if (pwm_signal_get_duty_cycle() > 0.95F) {
                // prevent very short off periods
                pwm_signal_set_duty_cycle(0.95F);
            }
            else if (pwm_signal_get_duty_cycle() < 0.05F) {
                // prevent very short on periods and switch completely off instead
                pwm_signal_stop();
                off_timestamp = uptime();
                // consider this as overvoltage in order to start again with 5% duty cycle
                dev_stat.set_error(ERR_PWM_SWITCH_OVERVOLTAGE);
                LOG_INF("PWM charger stop, no further derating possible.");
            }
            else {
                // decrease the power with large steps to prevent long-term overvoltages if
                // the PWM switch was started with full battery and high solar irradiation
                pwm_signal_duty_cycle_step(-10);
            }
        }
        else {
            // increase power (if not yet at 100% duty cycle)

            if (pwm_signal_get_duty_cycle() > 0.95F) {
                // prevent very short off periods and switch completely on instead
                pwm_signal_set_duty_cycle(1);
            }
            else {
                pwm_signal_duty_cycle_step(1);
            }
        }

        if (dev_stat.has_error(ERR_PWM_SWITCH_OVERVOLTAGE) &&
            bus->voltage < bus->sink_control_voltage() - 0.5F)
        {
            dev_stat.clear_error(ERR_PWM_SWITCH_OVERVOLTAGE);
        }
    }
    else {
        if (bus->sink_current_margin > 0          // charging allowed
            && bus->voltage < bus->sink_control_voltage()
            && ext_voltage > bus->voltage + offset_voltage_start
            && uptime() > (off_timestamp + restart_interval)
            && enable == true)
        {
            // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 50 ms
            adc_upper_alert_inhibit(ADC_POS(v_low), 50);

            if (dev_stat.has_error(ERR_PWM_SWITCH_OVERVOLTAGE)) {
                // start with minimum duty cycle in order to prevent another overvoltage event
                pwm_signal_start(0.05F);
            }
            else {
                pwm_signal_start(1.0F);
            }

            power_good_timestamp = uptime();
            LOG_INF("PWM charger start.");
        }
    }
}

void PwmSwitch::stop()
{
    pwm_signal_stop();
    off_timestamp = uptime();
}

float PwmSwitch::get_duty_cycle()
{
    return pwm_signal_get_duty_cycle();
}

#endif /* DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch)) */
