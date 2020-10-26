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
 *
 * All calculations assume a 16-bit ADC, even though the STM32 ADCs natively support 12-bit.
 * Using 16-bit for calculation allows to increase resolution by oversampling. Left-aligned
 * 12-bit ADC readings can be treated as 16-bit ADC readings.
 */

#ifndef DAQ_H_
#define DAQ_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr.h>

#include <stdint.h>

#define ADC_SCALE_FLOAT 65536.0F    // 16-bit full scale

#ifdef CONFIG_SOC_SERIES_STM32G4X
// Using internal reference buffer at VREF+ pin, set to 2048 mV
#define VREF (2048)
#elif defined(UNIT_TEST)
#define VREF (3300)
#else
// internal STM reference voltage (calibrated using 12-bit right-aligned readings)
#define VREF (VREFINT_VALUE * VREFINT_CAL / (adc_raw_filtered(ADC_POS(vref_mcu)) >> 4))
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
// cppcheck-suppress syntaxError
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
 * Convert 16-bit raw ADC reading to voltage
 *
 * @param raw 16-bit ADC reading
 * @param vref_mV Reference voltage in millivolts
 *
 * @return Voltage in volts
 */
static inline float adc_raw_to_voltage(int32_t raw, int32_t vref_mV)
{
    return (raw * vref_mV) / (ADC_SCALE_FLOAT * 1000);
}

/**
 * Convert voltage to 16-bit raw ADC reading
 *
 * @param voltage Voltage in volts
 * @param vref_mV Reference voltage in millivolts
 *
 * @return 16-bit ADC reading
 */
static inline int32_t adc_voltage_to_raw(float voltage, int32_t vref_mV)
{
    return voltage / vref_mV * ADC_SCALE_FLOAT * 1000;
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
 * @param lv_overvoltage Upper voltage limit
 * @param lv_undervoltage Lower voltage limit
 */
void daq_set_lv_limits(float lv_overvoltage, float lv_undervoltage);

/**
 * Set hv side (grid/solar) voltage limit where an alert should be triggered
 *
 * @param hv_overvoltage Upper voltage limit
 */
void daq_set_hv_limit(float hv_overvoltage);

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
