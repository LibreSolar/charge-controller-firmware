/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq.h"

#ifndef UNIT_TEST
#include <zephyr.h>
#endif

#include <math.h>       // log for thermistor calculation
#include <assert.h>

#include "mcu.h"
#include "setup.h"
#include "debug.h"
#include "board.h"        // contains defines for pins

#if DT_COMPAT_DCDC
static float dcdc_current_offset;
#endif
#if DT_OUTPUTS_PWM_SWITCH_PRESENT
static float pwm_current_offset;
#endif
static float load_current_offset;

// for ADC and DMA
volatile uint16_t adc_readings[NUM_ADC_CH] = {};
static volatile uint32_t adc_filtered[NUM_ADC_CH] = {};
static volatile AdcAlert adc_alerts_upper[NUM_ADC_CH] = {};
static volatile AdcAlert adc_alerts_lower[NUM_ADC_CH] = {};

extern DeviceStatus dev_stat;

#ifdef CONFIG_SOC_SERIES_STM32G4X
// Using internal reference buffer at VREF+ pin, set to 2048 mV
#define VREF (2048)
#else
// internal STM reference voltage
#define VREF (VREFINT_VALUE * VREFINT_CAL / adc_value(ADC_POS_VREF_MCU))
#endif

/**
 * Average value for ADC channel
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 */
static inline uint32_t adc_value(uint32_t channel)
{
    assert(channel < NUM_ADC_CH);
    return adc_filtered[channel] >> (4 + ADC_FILTER_CONST);
}

/**
 * Measured voltage for ADC channel after average
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 * @param vref reference voltage in millivolts
 *
 * @return voltage in millivolts
 */

static inline float adc_voltage(uint32_t channel, int32_t vref)
{
    return (adc_value(channel) * vref) / 4096;
}

/**
 * Measured current/voltage for ADC channel after average and scaling
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 * @param vref reference voltage in millivolts
 *
 * @return scaled final value
 */
static inline float adc_scaled(uint32_t channel, int32_t vref, const float gain)
{
    return adc_voltage(channel, vref) * (gain/1000.0);
}

static inline float ntc_temp(uint32_t channel, int32_t vref)
{
    /** \todo Improved (faster) temperature calculation:
       https://www.embeddedrelated.com/showarticle/91.php
    */

    float v_temp = adc_voltage(channel, vref);  // voltage read by ADC (mV)
    float rts = NTC_SERIES_RESISTOR * v_temp / (vref - v_temp); // resistance of NTC (Ohm)

    return 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
}

void calibrate_current_sensors()
{
    int vref = VREF;
#if DT_COMPAT_DCDC
    dcdc_current_offset = -adc_scaled(ADC_POS_I_DCDC, vref, ADC_GAIN_I_DCDC);
#endif
#if DT_OUTPUTS_PWM_SWITCH_PRESENT
    pwm_current_offset = -adc_scaled(ADC_POS_I_PWM, vref, ADC_GAIN_I_PWM);
#endif
    load_current_offset = -adc_scaled(ADC_POS_I_LOAD, vref, ADC_GAIN_I_LOAD);
}

void adc_update_value(unsigned int pos)
{
    // low pass filter with filter constant c = 1/(2^ADC_FILTER_CONST)
    // y(n) = c * x(n) + (c - 1) * y(n-1)
    // see also here: http://techteach.no/simview/lowpass_filter/doc/filter_algorithm.pdf

#if DT_OUTPUTS_PWM_SWITCH_PRESENT
    if (pos == ADC_POS_V_PWM || pos == ADC_POS_I_PWM) {
        // only read input voltage and current when switch is on or permanently off
        if (pwm_switch.signal_high() || pwm_switch.active() == false) {
            adc_filtered[pos] += (uint32_t)adc_readings[pos] - (adc_filtered[pos] >> ADC_FILTER_CONST);
        }
    }
    else
#endif
    {
        // adc_readings: 12-bit ADC values left-aligned in uint16_t
        adc_filtered[pos] += (uint32_t)adc_readings[pos] - (adc_filtered[pos] >> ADC_FILTER_CONST);
    }

    // check upper alerts
    adc_alerts_upper[pos].debounce_ms++;
    if (adc_alerts_upper[pos].callback != NULL &&
        adc_readings[pos] >= adc_alerts_upper[pos].limit)
    {
        if (adc_alerts_upper[pos].debounce_ms > 1) {
            // create function pointer and call function
            adc_alerts_upper[pos].callback();
        }
    }
    else if (adc_alerts_upper[pos].debounce_ms > 0) {
        // reset debounce ms counter only if already close to triggering to allow setting negative
        // values to specify a one-time inhibit delay
        adc_alerts_upper[pos].debounce_ms = 0;
    }

    // same for lower alerts
    adc_alerts_lower[pos].debounce_ms++;
    if (adc_alerts_lower[pos].callback != NULL &&
        adc_readings[pos] <= adc_alerts_lower[pos].limit)
    {
        if (adc_alerts_lower[pos].debounce_ms > 1) {
            adc_alerts_lower[pos].callback();
        }
    }
    else if (adc_alerts_lower[pos].debounce_ms > 0) {
        adc_alerts_lower[pos].debounce_ms = 0;
    }
}

void daq_update()
{
    int vref = VREF;

    // calculate lower voltage first, as it is needed for PWM terminal voltage calculation
    lv_bus.voltage = adc_scaled(ADC_POS_V_LOW, vref, ADC_GAIN_V_LOW);

#if DT_COMPAT_DCDC
    hv_bus.voltage = adc_scaled(ADC_POS_V_HIGH, vref, ADC_GAIN_V_HIGH);
#endif

#if DT_OUTPUTS_PWM_SWITCH_PRESENT
    pwm_switch.ext_voltage = lv_bus.voltage - vref * (ADC_OFFSET_V_PWM / 1000.0) -
        adc_scaled(ADC_POS_V_PWM, vref, ADC_GAIN_V_PWM);
#endif

    load.current = adc_scaled(ADC_POS_I_LOAD, vref, ADC_GAIN_I_LOAD) + load_current_offset;

#if DT_OUTPUTS_PWM_SWITCH_PRESENT
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power
    // calculation
    pwm_switch.current = -pwm_switch.get_duty_cycle() * (
        adc_scaled(ADC_POS_I_PWM, vref, ADC_GAIN_I_PWM) + pwm_current_offset);

    lv_terminal.current = -pwm_switch.current - load.current;

    pwm_switch.power = pwm_switch.bus->voltage * pwm_switch.current;
#endif

#if DT_COMPAT_DCDC
    dcdc_lv_port.current =
        adc_scaled(ADC_POS_I_DCDC, vref, ADC_GAIN_I_DCDC) + dcdc_current_offset;

    lv_terminal.current = dcdc_lv_port.current - load.current;

    hv_terminal.current = -dcdc_lv_port.current * lv_terminal.bus->voltage / hv_terminal.bus->voltage;

    dcdc_lv_port.power  = dcdc_lv_port.bus->voltage * dcdc_lv_port.current;
    hv_terminal.power   = hv_terminal.bus->voltage * hv_terminal.current;
#endif
    lv_terminal.power   = lv_terminal.bus->voltage * lv_terminal.current;
    load.power = load.bus->voltage * load.current;

#ifdef PIN_ADC_TEMP_BAT
    // battery temperature calculation
    float bat_temp = ntc_temp(ADC_POS_TEMP_BAT, vref);

    if (bat_temp > -50) {
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

#if DT_COMPAT_DCDC && defined(PIN_ADC_TEMP_FETS)
    // MOSFET temperature calculation
    dcdc.temp_mosfets = ntc_temp(ADC_POS_TEMP_FETS, vref);
#endif

    // internal MCU temperature
    uint16_t adcval = adc_value(ADC_POS_TEMP_MCU) * vref / VREFINT_VALUE;
    dev_stat.internal_temp = (TSENSE_CAL2_VALUE - TSENSE_CAL1_VALUE) /
        (TSENSE_CAL2 - TSENSE_CAL1) * (adcval - TSENSE_CAL1) + TSENSE_CAL1_VALUE;

    if (dev_stat.internal_temp > 80) {
        dev_stat.set_error(ERR_INT_OVERTEMP);
    }
    else if (dev_stat.internal_temp < 70 && (dev_stat.has_error(ERR_INT_OVERTEMP))) {
        // remove error flag with hysteresis of 10°C
        dev_stat.clear_error(ERR_INT_OVERTEMP);
    }
    // else: keep previous setting
}

void high_voltage_alert()
{
    // disable any sort of input
#if DT_COMPAT_DCDC
    dcdc.emergency_stop();
#endif
#if DT_OUTPUTS_PWM_SWITCH_PRESENT
    pwm_switch.emergency_stop();
#endif
    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    dev_stat.set_error(ERR_BAT_OVERVOLTAGE);

    print_error("High voltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS_V_LOW], adc_alerts_upper[ADC_POS_V_LOW].limit);
}

void low_voltage_alert()
{
    // the battery undervoltage must have been caused by a load current peak
    load.stop(ERR_LOAD_VOLTAGE_DIP);

    print_error("Low voltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS_V_LOW], adc_alerts_lower[ADC_POS_V_LOW].limit);
}

void adc_upper_alert_inhibit(int adc_pos, int timeout_ms)
{
    // set negative value so that we get a final debouncing of this timeout + the original
    // delay in the alert function (currently only waiting for 2 samples = 2 ms)
    adc_alerts_upper[adc_pos].debounce_ms = -timeout_ms;
}

uint16_t adc_get_alert_limit(float scale, float limit)
{
    const float adclimit = (UINT16_MAX >> 4); // 12 bits ADC resolution
    const float limit_scaled = limit * scale;
    // even if we have a higher voltage limit, we must limit it
    // to the max value the ADC will be able to deliver
    return (limit_scaled > adclimit ? 0x0FFF : (uint16_t)(limit_scaled)) << 4;
    // shift 4 bits left to generate left aligned 16bit value
}

void daq_set_lv_alerts(float upper, float lower)
{
    int vref = VREF;
    float scale =  ((4096* 1000) / (ADC_GAIN_V_LOW)) / vref;

    // LV side (battery) overvoltage alert
    adc_alerts_upper[ADC_POS_V_LOW].limit = adc_get_alert_limit(scale, upper);
    adc_alerts_upper[ADC_POS_V_LOW].callback = high_voltage_alert;

    // LV side (battery) undervoltage alert
    adc_alerts_lower[ADC_POS_V_LOW].limit = adc_get_alert_limit(scale, lower);
    adc_alerts_lower[ADC_POS_V_LOW].callback = low_voltage_alert;
}

#if defined(UNIT_TEST)

#include "../test/daq_stub.h"

void prepare_adc_readings(AdcValues values)
{
    adc_readings[ADC_POS_VREF_MCU] = (uint16_t)(1.224 / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_HIGH] = (uint16_t)((values.solar_voltage / (ADC_GAIN_V_HIGH)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_LOW] = (uint16_t)((values.battery_voltage / (ADC_GAIN_V_LOW)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_DCDC] = (uint16_t)((values.dcdc_current / (ADC_GAIN_I_DCDC)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_LOAD] = (uint16_t)((values.load_current / (ADC_GAIN_I_LOAD)) / 3.3 * 4096) << 4;
}

void prepare_adc_filtered()
{
    // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = adc_readings[i] << ADC_FILTER_CONST;
    }
}

void clear_adc_filtered()
{
    // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = 0;
    }
}
uint32_t get_adc_filtered(uint32_t channel)
{
    return adc_value(channel);
}

#endif /* UNIT_TEST */
