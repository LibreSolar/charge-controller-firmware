/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *
 * @brief Reads ADC via DMA and stores data into necessary structs
 */

#ifndef ADC_DMA_H
#define ADC_DMA_H

#ifdef __ZEPHYR__
#include <zephyr.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)

/** Struct to definie upper and lower limit alerts for any ADC channel
 */
typedef struct {
    void (*callback)();         ///< Function to be called when limits are exceeded
    uint16_t limit;             ///< ADC reading for lower limit
    int debounce_ms;            ///< Milliseconds delay for triggering alert
} AdcAlert;

/** Sets offset to actual measured value, i.e. sets zero current point.
 *
 * All input/output switches and consumers should be switched off before calling this function
 */
void calibrate_current_sensors();

/** Updates structures with data read from ADC
 */
void update_measurements();

#ifdef __ZEPHYR__

/** Restarts ADC sampling sequence (should be called by timer)
 */
void adc_trigger_conversion(struct k_timer *timer_id);

#else

/** Initializes registers and starts ADC timer
 */
void adc_timer_start(int freq_Hz);

#endif /* __ZEPHYR__ */

/** Sets necessary ADC registers
 */
void adc_setup(void);

/** Sets necessary DMA registers
 */
void dma_setup(void);

/** Read, filter and check raw ADC readings stored by DMA controller
 */
void adc_update_value(unsigned int pos);

/** Set lv side (battery) voltage limits where an alert should be triggered
 *
 * @param upper Upper voltage limit
 * @param lower Lower voltage limit
 */
void adc_set_lv_alerts(float upper, float lower);

/** Add an inhibit delay to the alerts to disable it temporarily
 *
 * @param adc_pos The position of the ADC measurement channel
 * @param timeout_ms Timeout in milliseconds
 */
void adc_upper_alert_inhibit(int adc_pos, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DMA */
