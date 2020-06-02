/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Data Acquisition (DAQ)
 *
 * Measurements are taken using ADC with DMA. The DAC is used to generate reference voltages
 * for bi-directional current measurement. Data is stored in structs, which are afterwards
 * used by the control algorithms
 */

#ifndef DAQ_H_
#define DAQ_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)

/**
 * Struct to definie upper and lower limit alerts for any ADC channel
 */
typedef struct {
    void (*callback)();         ///< Function to be called when limits are exceeded
    uint16_t limit;             ///< ADC reading for lower limit
    int16_t debounce_ms;        ///< Milliseconds delay for triggering alert
} AdcAlert;

/**
 * Set offset vs. actual measured value, i.e. sets zero current point.
 *
 * All input/output switches and consumers should be switched off before calling this function
 */
void calibrate_current_sensors(void);

/**
 * Updates structures with data read from ADC
 */
void daq_update(void);

/**
 * Initializes ADC, DAC and DMA
 */
void daq_setup(void);

/**
 * Read, filter and check raw ADC readings stored by DMA controller
 */
void adc_update_value(unsigned int pos);

/**
 * Set lv side (battery) voltage limits where an alert should be triggered
 *
 * @param upper Upper voltage limit
 * @param lower Lower voltage limit
 */
void daq_set_lv_alerts(float upper, float lower);

/**
 * Add an inhibit delay to the alerts to disable it temporarily
 *
 * @param adc_pos The position of the ADC measurement channel
 * @param timeout_ms Timeout in milliseconds
 */
void adc_upper_alert_inhibit(int adc_pos, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* DAQ_H_ */
