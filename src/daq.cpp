/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq.h"

#include <zephyr.h>

#ifndef UNIT_TEST
#include <logging/log.h>
LOG_MODULE_REGISTER(daq, CONFIG_LOG_DEFAULT_LEVEL);
#endif

#include <math.h>       // log for thermistor calculation
#include <assert.h>

#include "mcu.h"
#include "setup.h"

// typical value for Semitec 103AT-5 thermistor: 3435
#define NTC_BETA_VALUE 3435

#if DT_NODE_EXISTS(DT_PATH(dcdc))
static uint16_t dcdc_current_offset_raw;
#endif
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
static uint16_t pwm_current_offset_raw;
#endif
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
static uint16_t load_current_offset_raw;
#endif

// 16-bit ADC raw readings (actually left-aligned 12-bit, i.e. left-shifted by 4 bits)
volatile uint16_t adc_readings[NUM_ADC_CH] = {};

// filtered raw readings left-shifted by additional adc_filter_const[channel] bits
volatile uint32_t adc_filtered[NUM_ADC_CH] = {};

static volatile AdcAlert adc_alerts_upper[NUM_ADC_CH] = {};
static volatile AdcAlert adc_alerts_lower[NUM_ADC_CH] = {};

/*
 * Channel-specific ADC filter constant from devicetree
 *
 * multiplier = 1/(2^adc_filter_const[channel])
 */
#define ADC_FILTER_CONST(node_id) DT_PROP(node_id, filter_const),
static const uint8_t adc_filter_const[NUM_ADC_CH] = {
    DT_FOREACH_CHILD(DT_PATH(adc_inputs), ADC_FILTER_CONST)
};

/**
 * Average value for ADC channel
 *
 * @param channel valid ADC channel position using ADC_POS() macro
 */
static inline uint32_t adc_raw_filtered(uint32_t channel)
{
    return adc_filtered[channel] >> adc_filter_const[channel];
}

/**
 * Measured current/voltage for ADC channel after average and scaling
 *
 * @param channel valid ADC channel position using ADC_POS() macro
 * @param vref reference voltage in millivolts
 * @param offset offset added to raw ADC value before applying gain
 *
 * @return scaled final value in volts/amps
 */
static inline float adc_scaled(uint32_t channel, int32_t vref, const float gain,
    int32_t offset = 0)
{
    return adc_raw_to_voltage(adc_raw_filtered(channel) - offset, vref) * gain;
}

static inline float ntc_temp(uint32_t channel, int32_t vref, float ntc_series_resistor)
{
    /** \todo Improved (faster) temperature calculation:
       https://www.embeddedrelated.com/showarticle/91.php
    */

    // voltage read by ADC (mV)
    float v_temp = adc_raw_to_voltage(adc_raw_filtered(channel), vref) * 1000;

    // resistance of NTC (Ohm)
    float rts = ntc_series_resistor * v_temp / (vref - v_temp);

    return 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
}

void calibrate_current_sensors()
{
#if DT_NODE_EXISTS(DT_PATH(dcdc))
    dcdc_current_offset_raw = adc_raw_filtered(ADC_POS(i_dcdc));
#endif
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    pwm_current_offset_raw = adc_raw_filtered(ADC_POS(i_pwm));
#endif
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    load_current_offset_raw = adc_raw_filtered(ADC_POS(i_load));
#endif
}

void adc_update_value(unsigned int pos)
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    // only read input voltage and current when switch is on or permanently off
    if ((pos != ADC_POS(v_pwm) && pos != ADC_POS(i_pwm)) ||
        pwm_switch.signal_high() || pwm_switch.active() == false)
#endif
    {
        /*
         * Low-pass filtering of ADC raw readings
         * (see also: http://techteach.no/simview/lowpass_filter/doc/filter_algorithm.pdf)
         *
         * y(n) = c * x(n) + (1 - c) * y(n-1) with filter constant c = 1/(2^adc_filter_const[pos])
         *
         * Remarks regarding below implementation optimized for efficiency:
         *
         * 1. adc_readings has different fixed-point math format, so that it would have to be
         *    aligned with << adc_filter_const[pos] before calculation. Not aligning it is
         *    equivalent to multiplication with c:
         *    adc_readings[pos] == c * (adc_readings[pos] << adc_filter_const[pos])
         *
         * 2. c * adc_filtered[pos] == adc_filtered[pos] >> adc_filter_const[pos]
         */
        adc_filtered[pos] = (uint32_t)adc_readings[pos] +
            adc_filtered[pos] - (adc_filtered[pos] >> adc_filter_const[pos]);
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
    lv_bus.voltage = adc_scaled(ADC_POS(v_low), vref, ADC_GAIN(v_low));

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    hv_bus.voltage = adc_scaled(ADC_POS(v_high), vref, ADC_GAIN(v_high));
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    pwm_switch.ext_voltage = lv_bus.voltage -
        adc_scaled(ADC_POS(v_pwm), vref, ADC_GAIN(v_pwm), ADC_OFFSET(v_pwm));
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    load.current = adc_scaled(ADC_POS(i_load), vref, ADC_GAIN(i_load), load_current_offset_raw);
    float load_current = load.current;
#else
    float load_current = 0;     // value used below, so we still need to define the variable
#endif

    float lv_terminal_current = -load_current;

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power
    // calculation
    pwm_switch.current = -pwm_switch.get_duty_cycle() *
        adc_scaled(ADC_POS(i_pwm), vref, ADC_GAIN(i_pwm), pwm_current_offset_raw);

    lv_terminal_current -= pwm_switch.current;

    pwm_switch.power = pwm_switch.bus->voltage * pwm_switch.current;
#endif

#if DT_NODE_EXISTS(DT_PATH(dcdc))
    dcdc.inductor_current =
        adc_scaled(ADC_POS(i_dcdc), vref, ADC_GAIN(i_dcdc), dcdc_current_offset_raw);

    lv_terminal_current += dcdc.inductor_current;

    hv_terminal.current = -dcdc.inductor_current * lv_terminal.bus->voltage /
        hv_terminal.bus->voltage;

    dcdc.power  = dcdc.lvb->voltage * dcdc.inductor_current;
    hv_terminal.power   = hv_terminal.bus->voltage * hv_terminal.current;
#endif
    lv_terminal.current = lv_terminal_current;
    lv_terminal.power   = lv_terminal.bus->voltage * lv_terminal.current;

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    load.power = load.bus->voltage * load.current;
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(adc_inputs), temp_bat))
    // battery temperature calculation
    float bat_temp = ntc_temp(ADC_POS(temp_bat), vref, ADC_GAIN(temp_bat));

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

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(adc_inputs), temp_fets))
    // MOSFET temperature calculation
    dcdc.temp_mosfets = ntc_temp(ADC_POS(temp_fets), vref, ADC_GAIN(temp_fets));
#endif

    // internal MCU temperature (calibrated using 12-bit right-aligned readings)
    uint16_t adcval = (adc_raw_filtered(ADC_POS(temp_mcu)) >> 4) * vref / VREFINT_VALUE;
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

void lv_overvoltage_alert()
{
    // disable any sort of input
#if DT_NODE_EXISTS(DT_PATH(dcdc))
    dcdc.stop();
#endif
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    pwm_switch.stop();
#endif
    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    dev_stat.set_error(ERR_BAT_OVERVOLTAGE);

    LOG_ERR("Low-side overvoltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS(v_low)], adc_alerts_upper[ADC_POS(v_low)].limit);
}

void lv_undervoltage_alert()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    // the battery undervoltage must have been caused by a load current peak
    load.stop(ERR_LOAD_VOLTAGE_DIP);
#endif

    LOG_ERR("Low-side undervoltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS(v_low)], adc_alerts_lower[ADC_POS(v_low)].limit);
}

#if DT_NODE_EXISTS(DT_PATH(dcdc))
void hv_overvoltage_alert()
{
    dcdc.stop();

    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    dev_stat.set_error(ERR_DCDC_HS_OVERVOLTAGE);

    LOG_ERR("High-side overvoltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS(v_high)], adc_alerts_upper[ADC_POS(v_high)].limit);
}
#endif

void adc_upper_alert_inhibit(int adc_pos, int timeout_ms)
{
    // set negative value so that we get a final debouncing of this timeout + the original
    // delay in the alert function (currently only waiting for 2 samples = 2 ms)
    adc_alerts_upper[adc_pos].debounce_ms = -timeout_ms;
}

uint16_t adc_raw_clamp(float scale, float limit)
{
    // even if we have a higher voltage limit, we must limit it
    // to the max value the ADC will be able to deliver
    const float limit_scaled = limit * scale;
    return limit_scaled > (float)UINT16_MAX ? UINT16_MAX : (uint16_t)(limit_scaled);
}

void daq_set_lv_limits(float lv_overvoltage, float lv_undervoltage)
{
    float scale =  (((4096 << 4) * 1000) / ADC_GAIN(v_low)) / (float)VREF;

    // LV side (battery) overvoltage alert
    adc_alerts_upper[ADC_POS(v_low)].limit = adc_raw_clamp(scale, lv_overvoltage);
    adc_alerts_upper[ADC_POS(v_low)].callback = lv_overvoltage_alert;

    // LV side (battery) undervoltage alert
    adc_alerts_lower[ADC_POS(v_low)].limit = adc_raw_clamp(scale, lv_undervoltage);
    adc_alerts_lower[ADC_POS(v_low)].callback = lv_undervoltage_alert;
}

#if DT_NODE_EXISTS(DT_PATH(dcdc))
void daq_set_hv_limit(float hv_overvoltage)
{
    float scale =  (((4096 << 4) * 1000) / ADC_GAIN(v_high)) / (float)VREF;

    // HV side (solar/grid) overvoltage alert
    adc_alerts_upper[ADC_POS(v_high)].limit = adc_raw_clamp(scale, hv_overvoltage);
    adc_alerts_upper[ADC_POS(v_high)].callback = hv_overvoltage_alert;
}
#endif

#if defined(UNIT_TEST)

#include "../test/daq_stub.h"

void prepare_adc_readings(AdcValues values)
{
    adc_readings[ADC_POS(vref_mcu)] = (uint16_t)(1.224 / 3.3 * 4096) << 4;
    adc_readings[ADC_POS(v_high)] =
        (uint16_t)((values.solar_voltage / ADC_GAIN(v_high)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS(v_low)] =
        (uint16_t)((values.battery_voltage / ADC_GAIN(v_low)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS(i_dcdc)] =
        (uint16_t)((values.dcdc_current / ADC_GAIN(i_dcdc)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS(i_load)] =
        (uint16_t)((values.load_current / ADC_GAIN(i_load)) / 3.3 * 4096) << 4;
}

void prepare_adc_filtered()
{
    // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = adc_readings[i] << adc_filter_const[i];
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
    return adc_raw_filtered(channel);
}

#endif /* UNIT_TEST */
