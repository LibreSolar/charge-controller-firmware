/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DAQ_STUB_H_
#define DAQ_STUB_H_

/** Data to fill readings of the emulated DAQ during unit-tests
 */
typedef struct
{
    float solar_voltage;
    float battery_voltage;
    float dcdc_current;
    float load_current;
    float bat_temperature;
    float internal_temperature;
} AdcValues;

/** Set the readings of the emulated DAQ for the next samples */
void prepare_adc_readings(AdcValues values);

/** Set the filtered values of the emulated DAQ to the current readings */
void prepare_adc_filtered();

#endif
