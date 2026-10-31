/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq_common.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(daq, CONFIG_DAQ_LOG_LEVEL);

static inline bool channel_valid(const struct daq_common_config *config, int channel)
{
    return channel >= 0 && channel < config->num_channels;
}

static inline uint32_t filtered_raw(const struct device *dev, int channel)
{
    const struct daq_common_config *config = dev->config;
    const struct daq_common_data *data = dev->data;

    return data->channels[channel].filtered >> config->filter_const[channel];
}

static inline float raw_to_voltage(int32_t raw, int32_t vref_mv)
{
    return (raw * vref_mv) / ((float)DAQ_RAW_SCALE * 1000);
}

int daq_common_init(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    for (int i = 0; i < config->num_channels; i++) {
        data->channels[i].offset = config->offset[i];

#ifdef CONFIG_GPIO
        const struct gpio_dt_spec *enable = &config->enable_gpios[i];

        if (enable->port != NULL) {
            if (!gpio_is_ready_dt(enable)) {
                LOG_ERR("Enable GPIO of channel %d not ready", i);
                return -ENODEV;
            }
            gpio_pin_configure_dt(enable, GPIO_OUTPUT_ACTIVE);
        }
#endif
    }

    return 0;
}

static inline void check_alert(const struct device *dev, int channel, struct daq_alert *alert,
                               bool exceeded)
{
    daq_alert_callback_t callback = alert->callback;

    if (alert->debounce_ms < INT16_MAX) {
        alert->debounce_ms++;
    }

    if (callback != NULL && exceeded) {
        if (alert->debounce_ms > 1) {
            callback(dev, channel);
        }
    }
    else if (alert->debounce_ms > 0) {
        // reset debounce counter only if already close to triggering to allow setting negative
        // values to specify a one-time inhibit delay
        alert->debounce_ms = 0;
    }
}

void daq_common_process(const struct device *dev, int channel)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;
    struct daq_channel_data *ch = &data->channels[channel];
    const uint16_t raw = data->buf[ch->buf_idx];
    daq_sample_filter_t filter = data->sample_filter;

    if (filter == NULL || (data->sample_filter_mask & BIT(channel)) == 0 || filter(dev, channel)) {
        /*
         * Low-pass filtering of raw readings
         * (see also: http://techteach.no/simview/lowpass_filter/doc/filter_algorithm.pdf)
         *
         * y(n) = c * x(n) + (1 - c) * y(n-1) with filter constant c = 1/(2^filter_const)
         *
         * The filtered value is stored left-shifted by filter_const, so that the new raw reading
         * can be added without shifting (equivalent to the multiplication with c).
         */
        ch->filtered = raw + ch->filtered - (ch->filtered >> config->filter_const[channel]);
    }

    check_alert(dev, channel, &ch->alerts[DAQ_ALERT_UPPER],
                raw >= ch->alerts[DAQ_ALERT_UPPER].limit);
    check_alert(dev, channel, &ch->alerts[DAQ_ALERT_LOWER],
                raw <= ch->alerts[DAQ_ALERT_LOWER].limit);
}

int daq_get_raw(const struct device *dev, int channel)
{
    if (!channel_valid(dev->config, channel)) {
        return -EINVAL;
    }

    return filtered_raw(dev, channel);
}

int daq_get_vref_mv(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    const struct daq_common_data *data = dev->data;

    if (data->vref_mv != 0) {
        return data->vref_mv;
    }

    if (config->vref_channel < 0 || config->vrefint_cal == NULL) {
        return -ENOTSUP;
    }

    // factory calibration values are 12-bit right-aligned readings
    uint32_t vrefint_raw = filtered_raw(dev, config->vref_channel) >> 4;
    if (vrefint_raw == 0) {
        return -EAGAIN;
    }

    return config->vrefint_cal_mv * *config->vrefint_cal / vrefint_raw;
}

float daq_get_pin_voltage(const struct device *dev, int channel)
{
    int vref_mv = daq_get_vref_mv(dev);

    if (!channel_valid(dev->config, channel) || vref_mv < 0) {
        return 0.0F;
    }

    return raw_to_voltage(filtered_raw(dev, channel), vref_mv);
}

float daq_get_value(const struct device *dev, int channel)
{
    const struct daq_common_config *config = dev->config;
    const struct daq_common_data *data = dev->data;
    int vref_mv = daq_get_vref_mv(dev);

    if (!channel_valid(config, channel) || vref_mv < 0) {
        return 0.0F;
    }

    int32_t raw = (int32_t)filtered_raw(dev, channel) - data->channels[channel].offset;

    return raw_to_voltage(raw, vref_mv) * config->gain[channel];
}

int daq_get_internal_temp(const struct device *dev, float *temp)
{
    const struct daq_common_config *config = dev->config;
    int vref_mv = daq_get_vref_mv(dev);

    if (config->temp_channel < 0 || config->ts_cal1 == NULL || config->ts_cal2 == NULL) {
        return -ENOTSUP;
    }

    if (vref_mv < 0) {
        return vref_mv;
    }

    // calibration values are 12-bit right-aligned readings taken at ts_cal_mv reference voltage
    int32_t raw = (filtered_raw(dev, config->temp_channel) >> 4) * vref_mv / config->ts_cal_mv;
    int32_t cal1 = *config->ts_cal1;
    int32_t cal2 = *config->ts_cal2;

    *temp = (float)(config->ts_cal2_temp - config->ts_cal1_temp) / (cal2 - cal1) * (raw - cal1)
            + config->ts_cal1_temp;

    return 0;
}

int daq_calibrate_offset(const struct device *dev, int channel)
{
    struct daq_common_data *data = dev->data;

    if (!channel_valid(dev->config, channel)) {
        return -EINVAL;
    }

    data->channels[channel].offset = filtered_raw(dev, channel);

    return 0;
}

int daq_set_alert(const struct device *dev, int channel, enum daq_alert_type type, float limit,
                  daq_alert_callback_t callback)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;
    int vref_mv = daq_get_vref_mv(dev);

    if (!channel_valid(config, channel) || (type != DAQ_ALERT_UPPER && type != DAQ_ALERT_LOWER)) {
        return -EINVAL;
    }

    if (vref_mv < 0) {
        return vref_mv;
    }

    struct daq_channel_data *ch = &data->channels[channel];
    const float scale = ((float)DAQ_RAW_SCALE * 1000 / config->gain[channel]) / vref_mv;
    float raw = limit * scale + ch->offset;

    // even if we have a higher limit, we must limit it to the range of possible readings
    ch->alerts[type].limit = raw > UINT16_MAX ? UINT16_MAX : (raw < 0.0F ? 0 : (uint16_t)raw);
    ch->alerts[type].callback = callback;

    return 0;
}

int daq_inhibit_alert(const struct device *dev, int channel, enum daq_alert_type type,
                      int16_t timeout_ms)
{
    struct daq_common_data *data = dev->data;

    if (!channel_valid(dev->config, channel)
        || (type != DAQ_ALERT_UPPER && type != DAQ_ALERT_LOWER))
    {
        return -EINVAL;
    }

    // set negative value so that we get a final debouncing of this timeout + the original delay
    data->channels[channel].alerts[type].debounce_ms = -timeout_ms;

    return 0;
}

int daq_set_sample_filter(const struct device *dev, daq_sample_filter_t filter,
                          uint32_t channel_mask)
{
    struct daq_common_data *data = dev->data;

    data->sample_filter = NULL;
    data->sample_filter_mask = channel_mask;
    data->sample_filter = filter;

    return 0;
}

int daq_set_sample_callback(const struct device *dev, daq_sample_callback_t callback)
{
    struct daq_common_data *data = dev->data;

    data->sample_callback = callback;

    return 0;
}
