/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq.h"

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(daq_app, CONFIG_DAQ_LOG_LEVEL);

#include "setup.h"

// temperatures below this value are considered as invalid (e.g. sensor not connected)
#define NTC_TEMP_MIN (-50.0F)

// filter parameter c for additional battery voltage and current low-pass filter
// c = dt / (tau + dt) = 0.1s / (10s + 0.1s)
#define HV_BUS_VOLTAGE_FILTER_CONST      0.0099F
#define LV_BUS_VOLTAGE_FILTER_CONST      0.0099F
#define LV_TERMINAL_CURRENT_FILTER_CONST 0.0099F
#define PWM_CURRENT_FILTER_CONST         0.0625F // 1.5 seconds

static const struct device *const daq = DAQ_DEV;

#if BOARD_HAS_TEMP_BAT || BOARD_HAS_TEMP_FETS
/**
 * Read temperature from an NTC sensor
 *
 * @returns 0 on success or negative errno if the temperature is not available or implausible
 */
static int ntc_temp(const struct device *sensor, float *temp)
{
    struct sensor_value val;
    int err;

    err = sensor_sample_fetch(sensor);
    if (err != 0) {
        return err;
    }

    err = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &val);
    if (err != 0) {
        return err;
    }

    *temp = sensor_value_to_float(&val);

    return *temp > NTC_TEMP_MIN ? 0 : -ENODATA;
}
#endif

void calibrate_current_sensors()
{
#if BOARD_HAS_DCDC
    daq_calibrate_offset(daq, DAQ_CH(i_dcdc));
#endif
#if BOARD_HAS_PWM_PORT
    daq_calibrate_offset(daq, DAQ_CH(i_pwm));
#endif
#if BOARD_HAS_LOAD_OUTPUT
    daq_calibrate_offset(daq, DAQ_CH(i_load));
#endif
}

#if BOARD_HAS_PWM_PORT
static bool pwm_sample_filter(const struct device *dev, int channel)
{
    // only read input voltage and current when switch is on or permanently off
    return pwm_switch.signal_high() || pwm_switch.active() == false;
}
#endif

#ifdef CONFIG_CUSTOM_DCDC_CONTROLLER
static void dcdc_sample_callback(const struct device *dev)
{
    dcdc_low_level_controller();
}
#endif

void daq_setup()
{
    if (!device_is_ready(daq)) {
        LOG_ERR("DAQ device not ready");
        return;
    }

#if BOARD_HAS_PWM_PORT
    daq_set_sample_filter(daq, pwm_sample_filter, BIT(DAQ_CH(v_pwm)) | BIT(DAQ_CH(i_pwm)));
#endif

#ifdef CONFIG_CUSTOM_DCDC_CONTROLLER
    daq_set_sample_callback(daq, dcdc_sample_callback);
#endif

    k_sleep(K_MSEC(500)); // wait for ADC to collect some measurement values
    daq_update();
    calibrate_current_sensors();
    daq_update();
}

void daq_update()
{
    // calculate lower voltage first, as it is needed for PWM terminal voltage calculation
    lv_bus.voltage = daq_get_value(daq, DAQ_CH(v_low));

    if (lv_bus.voltage_filtered != 0.0F) {
        lv_bus.voltage_filtered = LV_BUS_VOLTAGE_FILTER_CONST * lv_bus.voltage
                                  + (1.0F - LV_BUS_VOLTAGE_FILTER_CONST) * lv_bus.voltage_filtered;
    }
    else {
        // initialize properly at startup
        lv_bus.voltage_filtered = lv_bus.voltage;
    }

#if BOARD_HAS_DCDC
    hv_bus.voltage = daq_get_value(daq, DAQ_CH(v_high));

    if (hv_bus.voltage_filtered != 0.0F) {
        hv_bus.voltage_filtered = HV_BUS_VOLTAGE_FILTER_CONST * hv_bus.voltage
                                  + (1.0F - HV_BUS_VOLTAGE_FILTER_CONST) * hv_bus.voltage_filtered;
    }
    else {
        // initialize properly at startup
        hv_bus.voltage_filtered = hv_bus.voltage;
    }
#endif

#if BOARD_HAS_PWM_PORT
    pwm_switch.ext_voltage = lv_bus.voltage - daq_get_value(daq, DAQ_CH(v_pwm));
#endif

#if BOARD_HAS_LOAD_OUTPUT
    load.current = daq_get_value(daq, DAQ_CH(i_load));
    float load_current = load.current;
#else
    float load_current = 0; // value used below, so we still need to define the variable
#endif

    float lv_terminal_current = -load_current;

#if BOARD_HAS_PWM_PORT
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power
    // calculation
    pwm_switch.current = -pwm_switch.get_duty_cycle() * daq_get_value(daq, DAQ_CH(i_pwm));
    pwm_switch.current_filtered = PWM_CURRENT_FILTER_CONST * pwm_switch.current
                                  + (1.0F - PWM_CURRENT_FILTER_CONST) * pwm_switch.current_filtered;

    lv_terminal_current -= pwm_switch.current;

    pwm_switch.power = pwm_switch.bus->voltage * pwm_switch.current;
#endif

#if BOARD_HAS_DCDC
    dcdc.inductor_current = daq_get_value(daq, DAQ_CH(i_dcdc));

    lv_terminal_current += dcdc.inductor_current;

    hv_terminal.current =
        -dcdc.inductor_current * lv_terminal.bus->voltage / hv_terminal.bus->voltage;

    dcdc.power = dcdc.lvb->voltage * dcdc.inductor_current;
    hv_terminal.power = hv_terminal.bus->voltage * hv_terminal.current;
#endif
    lv_terminal.current = lv_terminal_current;
    lv_terminal.power = lv_terminal.bus->voltage * lv_terminal.current;

    lv_terminal.current_filtered =
        LV_TERMINAL_CURRENT_FILTER_CONST * lv_terminal.current
        + (1.0F - LV_TERMINAL_CURRENT_FILTER_CONST) * lv_terminal.current_filtered;

#if BOARD_HAS_LOAD_OUTPUT
    load.power = load.bus->voltage * load.current;
#endif

#if BOARD_HAS_TEMP_BAT
    float bat_temp;

    if (ntc_temp(DEVICE_DT_GET(NTC_BAT_NODE), &bat_temp) == 0) {
        // external sensor connected: take measured value
        charger.bat_temperature = bat_temp;
        charger.ext_temp_sensor = true;
    }
    else {
        // no external sensor: assume typical room temperature
        charger.bat_temperature = 25;
        charger.ext_temp_sensor = false;
    }
#endif

#if BOARD_HAS_TEMP_FETS
    float fets_temp;

    if (ntc_temp(DEVICE_DT_GET(NTC_FETS_NODE), &fets_temp) == 0) {
        dcdc.temp_mosfets = fets_temp;
    }
#endif

    float internal_temp;
    if (daq_get_internal_temp(daq, &internal_temp) == 0) {
        dev_stat.internal_temp = internal_temp;
    }

    if (dev_stat.internal_temp > 80) {
        device_status_set_error(&dev_stat, ERR_INT_OVERTEMP);
    }
    else if (dev_stat.internal_temp < 70 && (device_status_has_error(&dev_stat, ERR_INT_OVERTEMP)))
    {
        // remove error flag with hysteresis of 10°C
        device_status_clear_error(&dev_stat, ERR_INT_OVERTEMP);
    }
    // else: keep previous setting
}

static void lv_overvoltage_alert(const struct device *dev, int channel)
{
    // disable any sort of input
#if BOARD_HAS_DCDC
    dcdc_stop(&dcdc);
#endif
#if BOARD_HAS_PWM_PORT
    pwm_switch.stop();
#endif
    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    device_status_set_error(&dev_stat, ERR_BAT_OVERVOLTAGE);

    LOG_ERR("Low-side overvoltage alert");
}

static void lv_undervoltage_alert(const struct device *dev, int channel)
{
#if BOARD_HAS_LOAD_OUTPUT
    // the battery undervoltage must have been caused by a load current peak
    load.stop(ERR_LOAD_VOLTAGE_DIP);
#endif

    LOG_ERR("Low-side undervoltage alert");
}

void daq_set_lv_limits(float lv_overvoltage, float lv_undervoltage)
{
    // LV side (battery) overvoltage alert
    daq_set_alert(daq, DAQ_CH(v_low), DAQ_ALERT_UPPER, lv_overvoltage, lv_overvoltage_alert);

    // LV side (battery) undervoltage alert
    daq_set_alert(daq, DAQ_CH(v_low), DAQ_ALERT_LOWER, lv_undervoltage, lv_undervoltage_alert);
}

#if BOARD_HAS_DCDC
static void hv_overvoltage_alert(const struct device *dev, int channel)
{
    dcdc_stop(&dcdc);

    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    device_status_set_error(&dev_stat, ERR_DCDC_HS_OVERVOLTAGE);

    LOG_ERR("High-side overvoltage alert");
}

void daq_set_hv_limit(float hv_overvoltage)
{
    // HV side (solar/grid) overvoltage alert
    int err =
        daq_set_alert(daq, DAQ_CH(v_high), DAQ_ALERT_UPPER, hv_overvoltage, hv_overvoltage_alert);
    if (err != 0) {
        LOG_ERR("Failed to set high-side overvoltage alert: %d", err);
    }
}
#endif
