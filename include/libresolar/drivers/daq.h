/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_DAQ_H_
#define LIBRESOLAR_DRIVERS_DAQ_H_

/**
 * @file
 * @brief Data acquisition (DAQ) driver API
 *
 * The DAQ samples all inputs defined as child nodes of the DAQ devicetree node with 1 kHz. Each
 * reading is low-pass filtered and checked against an upper and a lower limit, which can trigger
 * an alert callback.
 *
 * Raw readings are 16-bit values, even though the STM32 ADCs natively support 12-bit only.
 * Left-aligned 12-bit readings can be treated as 16-bit readings, which allows to increase the
 * resolution by filtering.
 *
 * Only a single DAQ instance is supported.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup daq_interface DAQ
 * @{
 */

/** Full-scale value of raw readings */
#define DAQ_RAW_SCALE 65536

/**
 * Get the channel index of a DAQ input
 *
 * @param node_id Node identifier of the DAQ device
 * @param name Lower-case-and-underscores name of the child node, e.g. v_low
 */
#define DAQ_CHANNEL(node_id, name) DT_NODE_CHILD_IDX(DT_CHILD(node_id, name))

/** Alert types */
enum daq_alert_type
{
    /** Alert if the reading is greater than or equal to the limit */
    DAQ_ALERT_UPPER,
    /** Alert if the reading is less than or equal to the limit */
    DAQ_ALERT_LOWER,
};

/**
 * Alert callback
 *
 * Called from interrupt context for every sample after the limit was exceeded for two
 * consecutive samples, so it must be short.
 *
 * @param dev DAQ device
 * @param channel Channel index
 */
typedef void (*daq_alert_callback_t)(const struct device *dev, int channel);

/**
 * Sample filter callback
 *
 * Called from interrupt context for each new sample of the channels selected with
 * daq_set_sample_filter().
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @return false if the sample should not be used for the filtered value
 */
typedef bool (*daq_sample_filter_t)(const struct device *dev, int channel);

/**
 * Sample callback
 *
 * Called from interrupt context after a new sample of all channels was processed, so it must be
 * very short.
 *
 * @param dev DAQ device
 */
typedef void (*daq_sample_callback_t)(const struct device *dev);

/**
 * Get the filtered raw reading of a channel
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @return 16-bit raw reading or negative error code
 */
int daq_get_raw(const struct device *dev, int channel);

/**
 * Get the reference voltage of the ADC
 *
 * @param dev DAQ device
 *
 * @return Reference voltage in millivolts or negative error code
 * @retval -EAGAIN if the reference voltage is calculated from readings and no reading is available
 *                 yet
 */
int daq_get_vref_mv(const struct device *dev);

/**
 * Get the filtered voltage at the ADC pin of a channel
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @return Voltage in volts (0 if the reference voltage is not yet known)
 */
float daq_get_pin_voltage(const struct device *dev, int channel);

/**
 * Get the filtered and scaled value of a channel
 *
 * The offset is subtracted from the raw reading before the gain is applied.
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @return Scaled value, e.g. in volts or amps (0 if the reference voltage is not yet known)
 */
float daq_get_value(const struct device *dev, int channel);

/**
 * Get the internal temperature of the MCU
 *
 * Requires a child node named temp-mcu.
 *
 * @param dev DAQ device
 * @param temp Pointer to store the temperature in °C
 *
 * @retval 0 on success
 * @retval -ENOTSUP if the DAQ doesn't measure the internal temperature
 * @retval -EAGAIN if no reading is available yet
 */
int daq_get_internal_temp(const struct device *dev, float *temp);

/**
 * Use the current filtered reading of a channel as its offset, i.e. as zero point
 *
 * All switches and consumers should be off, so that no current flows.
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 */
int daq_calibrate_offset(const struct device *dev, int channel);

/**
 * Set an alert
 *
 * The limit is converted to a raw reading using the gain, offset and the current reference
 * voltage. It is clamped to the range of valid readings.
 *
 * @param dev DAQ device
 * @param channel Channel index
 * @param type Alert type
 * @param limit Limit as scaled value, e.g. in volts or amps
 * @param callback Callback to be called if the limit is exceeded, or NULL to disable the alert
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 * @retval -EAGAIN if the reference voltage is not yet known
 */
int daq_set_alert(const struct device *dev, int channel, enum daq_alert_type type, float limit,
                  daq_alert_callback_t callback);

/**
 * Inhibit an alert temporarily
 *
 * @param dev DAQ device
 * @param channel Channel index
 * @param type Alert type
 * @param timeout_ms Time to inhibit the alert in milliseconds
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 */
int daq_inhibit_alert(const struct device *dev, int channel, enum daq_alert_type type,
                      int16_t timeout_ms);

/**
 * Set a filter to decide if a sample is used for the filtered value
 *
 * Alerts are checked for all samples.
 *
 * @param dev DAQ device
 * @param filter Sample filter callback or NULL to use all samples
 * @param channel_mask Bit mask of the channels the filter applies to
 *
 * @retval 0 on success
 */
int daq_set_sample_filter(const struct device *dev, daq_sample_filter_t filter,
                          uint32_t channel_mask);

/**
 * Set a callback to be called after each new sample of all channels
 *
 * @param dev DAQ device
 * @param callback Sample callback or NULL to disable
 *
 * @retval 0 on success
 */
int daq_set_sample_callback(const struct device *dev, daq_sample_callback_t callback);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_DAQ_H_ */
