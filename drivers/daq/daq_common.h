/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_DAQ_COMMON_H_
#define LIBRESOLAR_DRIVERS_DAQ_COMMON_H_

/**
 * @file
 * @brief Common part of the DAQ drivers (filtering, alerts and scaling)
 *
 * Backends store new samples in a buffer and call daq_common_process() for each channel. The
 * config and data structs of the backends must start with struct daq_common_config and
 * struct daq_common_data.
 */

#include <libresolar/drivers/daq.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#include <stdint.h>

/** Sampling period of all DAQ backends (alert debouncing assumes 1 ms) */
#define DAQ_SAMPLE_PERIOD_MS 1

struct daq_alert
{
    volatile daq_alert_callback_t callback;
    volatile uint16_t limit;
    volatile int16_t debounce_ms;
};

struct daq_channel_data
{
    /** Filtered raw reading left-shifted by filter_const */
    volatile uint32_t filtered;
    struct daq_alert alerts[2];
    /** Offset to be subtracted from the raw reading */
    uint16_t offset;
    /** Position of the channel in the sample buffer */
    uint8_t buf_idx;
};

struct daq_common_config
{
    const uint8_t *filter_const;
    const float *gain;
    const uint16_t *offset;
    const uint8_t *adc_channel;
    const struct gpio_dt_spec *enable_gpios;
    /** Factory calibration values (NULL if not available) */
    const uint16_t *vrefint_cal;
    const uint16_t *ts_cal1;
    const uint16_t *ts_cal2;
    uint16_t vrefint_cal_mv;
    uint16_t ts_cal_mv;
    int16_t ts_cal1_temp;
    int16_t ts_cal2_temp;
    /** Channel indices of internal reference and temperature sensor (-1 if not available) */
    int8_t vref_channel;
    int8_t temp_channel;
    uint8_t num_channels;
};

struct daq_common_data
{
    struct daq_channel_data *channels;
    /** Buffer with the latest raw sample of each channel */
    volatile uint16_t *buf;
    daq_sample_filter_t sample_filter;
    uint32_t sample_filter_mask;
    daq_sample_callback_t sample_callback;
    /** Fixed reference voltage or 0 if calculated from the internal reference */
    uint16_t vref_mv;
};

/**
 * Initialize the common part (offsets and enable GPIOs)
 *
 * The backend has to set the buffer and the buffer indices afterwards.
 */
int daq_common_init(const struct device *dev);

/** Filter the latest sample of a channel and check its alerts */
void daq_common_process(const struct device *dev, int channel);

/** Call the sample callback after a new sample of all channels was processed */
static inline void daq_common_sample_done(const struct device *dev)
{
    const struct daq_common_data *data = dev->data;
    daq_sample_callback_t callback = data->sample_callback;

    if (callback != NULL) {
        callback(dev);
    }
}

/*
 * Helper macros to create the common config of a backend from the devicetree
 */

#define DAQ_FILTER_CONST(child) DT_PROP(child, filter_const),
#define DAQ_GAIN(child)         ((float)DT_PROP(child, multiplier) / DT_PROP(child, divider)),
#define DAQ_OFFSET(child)       DT_PROP_OR(child, offset, 0),
#define DAQ_ADC_CHANNEL(child)  DT_IO_CHANNELS_INPUT(child),
#define DAQ_ENABLE_GPIO(child)  GPIO_DT_SPEC_GET_OR(child, enable_gpios, { 0 }),

#define DAQ_CHILD_IDX_OR_NONE(node_id, name) \
    COND_CODE_1(DT_NODE_EXISTS(DT_CHILD(node_id, name)), \
                (DT_NODE_CHILD_IDX(DT_CHILD(node_id, name))), (-1))

/** Define the arrays used by DAQ_COMMON_CONFIG_INIT() */
#define DAQ_COMMON_DEFINE(node_id) \
    BUILD_ASSERT(DT_CHILD_NUM(node_id) <= 32, "Too many DAQ channels"); \
    static const uint8_t daq_filter_const[] = { DT_FOREACH_CHILD(node_id, DAQ_FILTER_CONST) }; \
    static const float daq_gain[] = { DT_FOREACH_CHILD(node_id, DAQ_GAIN) }; \
    static const uint16_t daq_offset[] = { DT_FOREACH_CHILD(node_id, DAQ_OFFSET) }; \
    static const uint8_t daq_adc_channel[] = { DT_FOREACH_CHILD(node_id, DAQ_ADC_CHANNEL) }; \
    static const struct gpio_dt_spec daq_enable_gpios[] = { DT_FOREACH_CHILD(node_id, \
                                                                             DAQ_ENABLE_GPIO) }; \
    static struct daq_channel_data daq_channels[DT_CHILD_NUM(node_id)]; \
    static volatile uint16_t daq_buf[DT_CHILD_NUM(node_id)]

/** Initializer for struct daq_common_config (without calibration values) */
#define DAQ_COMMON_CONFIG_INIT(node_id) \
    .filter_const = daq_filter_const, .gain = daq_gain, .offset = daq_offset, \
    .adc_channel = daq_adc_channel, .enable_gpios = daq_enable_gpios, \
    .vref_channel = DAQ_CHILD_IDX_OR_NONE(node_id, vref_mcu), \
    .temp_channel = DAQ_CHILD_IDX_OR_NONE(node_id, temp_mcu), \
    .num_channels = DT_CHILD_NUM(node_id)

/** Initializer for struct daq_common_data */
#define DAQ_COMMON_DATA_INIT() .channels = daq_channels, .buf = daq_buf

#endif /* LIBRESOLAR_DRIVERS_DAQ_COMMON_H_ */
