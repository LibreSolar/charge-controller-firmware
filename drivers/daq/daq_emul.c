/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Emulated DAQ driver for tests on native_sim
 */

#define DT_DRV_COMPAT libresolar_daq_emul

#include "daq_common.h"

#include <libresolar/drivers/daq_emul.h>

#include <zephyr/device.h>

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1, "Only one DAQ instance supported");

#define DAQ_NODE DT_DRV_INST(0)

/*
 * Typical calibration values of an STM32 temperature sensor (0.76 V at 30 °C, 2.5 mV/°C)
 * as 12-bit readings at 3.3 V
 */
static const uint16_t ts_cal1 = 4096 * 760 / 3300;
static const uint16_t ts_cal2 = 4096 * 1010 / 3300;

DAQ_COMMON_DEFINE(DAQ_NODE);

static const struct daq_common_config daq_emul_config = {
    DAQ_COMMON_CONFIG_INIT(DAQ_NODE),
    .ts_cal1 = &ts_cal1,
    .ts_cal2 = &ts_cal2,
    .ts_cal_mv = 3300,
    .ts_cal1_temp = 30,
    .ts_cal2_temp = 130,
};

static struct daq_common_data daq_emul_data = {
    DAQ_COMMON_DATA_INIT(),
    .vref_mv = DAQ_EMUL_VREF_MV,
};

static inline bool channel_valid(const struct device *dev, int channel)
{
    const struct daq_common_config *config = dev->config;

    return channel >= 0 && channel < config->num_channels;
}

int daq_emul_set_raw(const struct device *dev, int channel, uint16_t raw)
{
    struct daq_common_data *data = dev->data;

    if (!channel_valid(dev, channel)) {
        return -EINVAL;
    }

    data->buf[data->channels[channel].buf_idx] = raw;

    return 0;
}

int daq_emul_set_value(const struct device *dev, int channel, float value)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    if (!channel_valid(dev, channel)) {
        return -EINVAL;
    }

    // 12-bit resolution as provided by the ADC
    float raw12 = (value / config->gain[channel]) / (DAQ_EMUL_VREF_MV / 1000.0F) * 4096;
    raw12 = CLAMP(raw12, -4096.0F, 4096.0F);

    int32_t raw = (int32_t)raw12 * 16 + data->channels[channel].offset;

    return daq_emul_set_raw(dev, channel, CLAMP(raw, 0, UINT16_MAX));
}

int daq_emul_process(const struct device *dev, int channel)
{
    if (!channel_valid(dev, channel)) {
        return -EINVAL;
    }

    daq_common_process(dev, channel);

    return 0;
}

void daq_emul_process_all(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;

    for (int i = 0; i < config->num_channels; i++) {
        daq_common_process(dev, i);
    }

    daq_common_sample_done(dev);
}

void daq_emul_preset_filtered(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    for (int i = 0; i < config->num_channels; i++) {
        struct daq_channel_data *ch = &data->channels[i];

        ch->filtered = (uint32_t)data->buf[ch->buf_idx] << config->filter_const[i];
    }
}

void daq_emul_reset_filtered(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    for (int i = 0; i < config->num_channels; i++) {
        data->channels[i].filtered = 0;
    }
}

static int daq_emul_init(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    for (int i = 0; i < config->num_channels; i++) {
        data->channels[i].buf_idx = i;
    }

    return daq_common_init(dev);
}

DEVICE_DT_INST_DEFINE(0, daq_emul_init, NULL, &daq_emul_data, &daq_emul_config, POST_KERNEL,
                      CONFIG_DAQ_INIT_PRIORITY, NULL);
