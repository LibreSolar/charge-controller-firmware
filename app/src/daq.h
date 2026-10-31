/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Data Acquisition (DAQ)
 *
 * Measurements are taken by the DAQ driver. This module converts them to the measurement values
 * stored in the structs of the power ports and DC buses, which are afterwards used by the control
 * algorithms.
 */

#ifndef DAQ_H_
#define DAQ_H_

#include <libresolar/drivers/daq.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

/** DAQ devicetree node */
#define DAQ_NODE DT_PATH(adc_inputs)

/** DAQ device */
#define DAQ_DEV DEVICE_DT_GET(DAQ_NODE)

/**
 * Get the channel index of a DAQ input
 *
 * @param name Lower-case-and-underscores name of the child node of the DAQ node, e.g. v_low
 */
#define DAQ_CH(name) DAQ_CHANNEL(DAQ_NODE, name)

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
 * Waits for initial measurements and calibrates the current sensors
 */
void daq_setup(void);

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

#ifdef __cplusplus
}
#endif

#endif /* DAQ_H_ */
