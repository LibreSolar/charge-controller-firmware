/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_DAQ_EMUL_H_
#define LIBRESOLAR_DRIVERS_DAQ_EMUL_H_

/**
 * @file
 * @brief Test API of the emulated DAQ driver
 *
 * The emulated DAQ doesn't sample by itself. Readings are set with daq_emul_set_raw() or
 * daq_emul_set_value() and processed like a new sample of the ADC with daq_emul_process().
 *
 * The reference voltage is fixed to DAQ_EMUL_VREF_MV.
 */

#include <zephyr/device.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed reference voltage of the emulated DAQ in millivolts */
#define DAQ_EMUL_VREF_MV 3300

/**
 * Set the raw reading of a channel for the next sample
 *
 * @param dev DAQ device
 * @param channel Channel index
 * @param raw 16-bit raw reading
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 */
int daq_emul_set_raw(const struct device *dev, int channel, uint16_t raw);

/**
 * Set the reading of a channel for the next sample as scaled value
 *
 * The value is converted to a 12-bit reading, like a real ADC would provide.
 *
 * @param dev DAQ device
 * @param channel Channel index
 * @param value Scaled value, e.g. in volts or amps
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 */
int daq_emul_set_value(const struct device *dev, int channel, float value);

/**
 * Process the current reading of a channel like a new sample (filter and alerts)
 *
 * @param dev DAQ device
 * @param channel Channel index
 *
 * @retval 0 on success
 * @retval -EINVAL if the channel is invalid
 */
int daq_emul_process(const struct device *dev, int channel);

/**
 * Process the current readings of all channels and call the sample callback
 *
 * @param dev DAQ device
 */
void daq_emul_process_all(const struct device *dev);

/**
 * Set the filtered values of all channels to the current readings
 *
 * @param dev DAQ device
 */
void daq_emul_preset_filtered(const struct device *dev);

/**
 * Reset the filtered values of all channels to zero
 *
 * @param dev DAQ device
 */
void daq_emul_reset_filtered(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* LIBRESOLAR_DRIVERS_DAQ_EMUL_H_ */
