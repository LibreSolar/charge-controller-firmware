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

#include <zephyr.h>

#include <stdint.h>

#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)

#ifdef CONFIG_SOC_SERIES_STM32G4X
// Using internal reference buffer at VREF+ pin, set to 2048 mV
#define VREF (2048)
#elif defined(UNIT_TEST)
#define VREF (3300)
#else
// internal STM reference voltage
#define VREF (VREFINT_VALUE * VREFINT_CAL / adc_raw_filtered(ADC_POS(vref_mcu)))
#endif

#define ADC_GAIN(name) ((float)DT_PROP(DT_CHILD(DT_PATH(adc_inputs), name), multiplier) / \
    DT_PROP(DT_CHILD(DT_PATH(adc_inputs), name), divider))

#define ADC_OFFSET(name) (DT_PROP(DT_CHILD(DT_PATH(adc_inputs), name), offset))

/*
 * Find out the position in the ADC reading array for a channel identified by its Devicetree node
 */
#define ADC_POS(node) DT_N_S_adc_inputs_S_##node##_ADC_POS

/*
 * Creates a unique name for below enum
 */
#define ADC_ENUM(node) node##_ADC_POS,

/*
 * Enum for numbering of ADC channels as they are written by the DMA controller
 *
 * The channels must be specified in ascending order in the board.dts file.
 */
enum {
    DT_FOREACH_CHILD(DT_PATH(adc_inputs), ADC_ENUM)
    NUM_ADC_CH          // trick to get the number of elements
};

/**
 * Struct to define upper and lower limit alerts for any ADC channel
 */
typedef struct {
    void (*callback)();         ///< Function to be called when limits are exceeded
    uint16_t limit;             ///< ADC reading for lower limit
    int16_t debounce_ms;        ///< Milliseconds delay for triggering alert
} AdcAlert;

/**
 * Convert raw ADC reading to voltage
 *
 * @param raw 12-bit ADC reading (right-aligned)
 * @param vref_mV Reference voltage in millivolts
 *
 * @return Voltage in Volts
 */
static inline float adc_raw_to_voltage(int32_t raw, int32_t vref_mV)
{
    return (raw * vref_mV) / (4096.0F * 1000);
}

/**
 * Convert voltage to raw ADC reading
 *
 * @param voltage Voltage in Volts
 * @param vref_mV Reference voltage in millivolts
 *
 * @return 12-bit ADC reading (right-aligned)
 */
static inline int32_t adc_voltage_to_raw(float voltage, int32_t vref_mV)
{
    return voltage / vref_mV * 4096.0F * 1000;
}

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
