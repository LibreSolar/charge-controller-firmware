/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pwm_switch.h"

#include <zephyr/kernel.h>

#include <stdio.h>
#include <time.h>

#include "daq.h"
#include "helper.h"
#include "setup.h"

#include <libresolar/drivers/power_switch.h>

LOG_MODULE_REGISTER(pwm_switch, CONFIG_PWM_LOG_LEVEL);

#if BOARD_HAS_PWM_PORT

#define PWM_CURRENT_MAX (DT_PROP(PWM_SWITCH_NODE, current_max))

// duty cycle step corresponding to 100 us on-time
#define PWM_DUTY_STEP (DT_PROP(PWM_SWITCH_NODE, frequency) / 10000.0F)

bool PwmSwitch::active()
{
    return power_switch_is_on(power_switch);
}

bool PwmSwitch::signal_high()
{
    return power_switch_is_conducting(power_switch);
}

PwmSwitch::PwmSwitch(struct dc_bus *dc_bus, const struct device *sw) : power_switch(sw)
{
    power_port_init(this, dc_bus, false);

    if (!device_is_ready(power_switch)) {
        LOG_ERR("PWM switch not ready");
        enable = false;
        return;
    }

    power_switch_set_callback(power_switch, POWER_SWITCH_EVENT_PWM_EDGE, pwm_edge_callback, this);
}

void PwmSwitch::pwm_edge_callback(const struct device *dev, uint32_t events, void *user_data)
{
    // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 10 ms
    // at each rising edge if switch is not continuously on
    daq_inhibit_alert(DAQ_DEV, DAQ_CH(v_low), DAQ_ALERT_UPPER, 10);
}

void PwmSwitch::duty_cycle_step(int steps)
{
    // setting is ignored by the driver if the new duty cycle is out of range
    power_switch_set_duty_cycle(power_switch, get_duty_cycle() + steps * PWM_DUTY_STEP);
}

void PwmSwitch::test()
{
    if (active() && enable == false) {
        power_switch_off(power_switch);
        off_timestamp = uptime();
        LOG_INF("PWM test mode stop.");
    }
    else if (!active() && enable == true) {
        // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 50 ms
        daq_inhibit_alert(DAQ_DEV, DAQ_CH(v_low), DAQ_ALERT_UPPER, 50);
        power_switch_set_duty_cycle(power_switch, 0.9F);
        power_switch_on(power_switch);
        LOG_INF("PWM test mode start.");
    }
}

void PwmSwitch::control()
{
    if (active()) {
        if (current < -0.1F) {
            power_good_timestamp = uptime(); // reset the time
        }

        if (neg_current_limit == 0 || (uptime() - power_good_timestamp > 10) // low power since 10s
            || current > 0.5F      // discharging battery into solar panel --> stop
            || bus->voltage < 9.0F // not enough voltage for MOSFET drivers anymore
            || enable == false)
        {
            power_switch_off(power_switch);
            off_timestamp = uptime();
            LOG_INF("PWM charger stop, current = %d mA", (int)(current * 1000.0F));
        }
        else if (bus->voltage > dc_bus_sink_control_voltage(bus, 0) + 0.3F) {
            power_switch_off(power_switch);
            off_timestamp = uptime();
            device_status_set_error(&dev_stat, ERR_PWM_SWITCH_OVERVOLTAGE);
            LOG_INF("PWM charger stop, overvoltage.");
        }
        else if (bus->voltage > dc_bus_sink_control_voltage(bus, 0) // bus voltage above target
                 || current < neg_current_limit                     // port current limit exceeded
                 || current < -PWM_CURRENT_MAX)                     // PCB current limit exceeded
        {
            // decrease power, as limits were reached

            // the gate driver switch-off time is quite high (fall time around 1ms), so very short
            // on or off periods (duty cycle close to 0 and 1) should be avoided
            if (get_duty_cycle() > 0.95F) {
                // prevent very short off periods
                power_switch_set_duty_cycle(power_switch, 0.95F);
            }
            else if (get_duty_cycle() < 0.05F) {
                // prevent very short on periods and switch completely off instead
                power_switch_off(power_switch);
                off_timestamp = uptime();
                // consider this as overvoltage in order to start again with 5% duty cycle
                device_status_set_error(&dev_stat, ERR_PWM_SWITCH_OVERVOLTAGE);
                LOG_INF("PWM charger stop, no further derating possible.");
            }
            else {
                // decrease the power with large steps to prevent long-term overvoltages if
                // the PWM switch was started with full battery and high solar irradiation
                duty_cycle_step(-10);
            }
        }
        else {
            // increase power (if not yet at 100% duty cycle)

            if (get_duty_cycle() > 0.95F) {
                // prevent very short off periods and switch completely on instead
                power_switch_set_duty_cycle(power_switch, 1);
            }
            else {
                duty_cycle_step(1);
            }
        }

        if (device_status_has_error(&dev_stat, ERR_PWM_SWITCH_OVERVOLTAGE)
            && bus->voltage < dc_bus_sink_control_voltage(bus, 0) - 0.5F)
        {
            device_status_clear_error(&dev_stat, ERR_PWM_SWITCH_OVERVOLTAGE);
        }
    }
    else {
        if (bus->sink_current_margin > 0 && // charging allowed
            bus->voltage < dc_bus_sink_control_voltage(bus, 0)
            && ext_voltage > bus->voltage + offset_voltage_start
            && uptime() > (off_timestamp + restart_interval) && enable == true)
        {
            // turning the PWM switch on creates a short voltage rise, so inhibit alerts by 50 ms
            daq_inhibit_alert(DAQ_DEV, DAQ_CH(v_low), DAQ_ALERT_UPPER, 50);

            if (device_status_has_error(&dev_stat, ERR_PWM_SWITCH_OVERVOLTAGE)) {
                // start with minimum duty cycle in order to prevent another overvoltage event
                power_switch_set_duty_cycle(power_switch, 0.05F);
                power_switch_on(power_switch);
            }
            else {
                power_switch_set_duty_cycle(power_switch, 1.0F);
                power_switch_on(power_switch);
            }

            power_good_timestamp = uptime();
            LOG_INF("PWM charger start.");
        }
    }
}

void PwmSwitch::stop()
{
    power_switch_off(power_switch);
    off_timestamp = uptime();
}

float PwmSwitch::get_duty_cycle()
{
    return power_switch_get_duty_cycle(power_switch);
}

#endif /* BOARD_HAS_PWM_PORT */
