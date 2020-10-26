/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DAQ_STUB_H_
#define DAQ_STUB_H_

/** Data to fill adc_filtered array during unit-tests
 */
typedef struct {
    float solar_voltage;
    float battery_voltage;
    float dcdc_current;
    float load_current;
    float bat_temperature;
    float internal_temperature;
} AdcValues;

void prepare_adc_readings(AdcValues values);

void prepare_adc_filtered();
void clear_adc_filtered();
uint32_t get_adc_filtered(uint32_t channel);
uint16_t adc_raw_clamp(float scale, float limit);

#endif
