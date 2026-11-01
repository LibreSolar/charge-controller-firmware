/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NTC thermistor measured via a DAQ channel
 *
 * The NTC is connected between the ADC input and ground with a pull-up resistor to the ADC
 * reference voltage. The temperature is calculated from the filtered DAQ reading using the beta
 * equation.
 *
 * Zephyr's NTC thermistor driver can't be used, as the ADC is continuously sampled by the DAQ.
 */

#define DT_DRV_COMPAT libresolar_daq_ntc_thermistor

#include <libresolar/drivers/daq.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <math.h>

LOG_MODULE_REGISTER(daq_ntc, CONFIG_SENSOR_LOG_LEVEL);

#define ZERO_CELSIUS_IN_KELVIN 273.15F
#define NOMINAL_TEMP_KELVIN    (ZERO_CELSIUS_IN_KELVIN + 25.0F)

struct daq_ntc_config
{
    const struct device *daq;
    uint32_t pullup_ohm;
    uint32_t nominal_ohm;
    uint16_t beta;
    uint8_t channel;
};

struct daq_ntc_data
{
    float temp;
};

static int daq_ntc_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    const struct daq_ntc_config *config = dev->config;
    struct daq_ntc_data *data = dev->data;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP) {
        return -ENOTSUP;
    }

    int vref_mv = daq_get_vref_mv(config->daq);
    if (vref_mv < 0) {
        return vref_mv;
    }

    float vref = vref_mv / 1000.0F;
    float v_ntc = daq_get_pin_voltage(config->daq, config->channel);

    if (v_ntc <= 0.0F || v_ntc >= vref) {
        /*
         * Temperature can't be calculated. Note that a missing NTC usually results in a reading
         * slightly below the reference voltage and an implausibly low temperature instead.
         */
        return -ENODATA;
    }

    float r_ntc = config->pullup_ohm * v_ntc / (vref - v_ntc);

    data->temp =
        1.0F
            / (1.0F / NOMINAL_TEMP_KELVIN + logf(r_ntc / config->nominal_ohm) / (float)config->beta)
        - ZERO_CELSIUS_IN_KELVIN;

    return 0;
}

static int daq_ntc_channel_get(const struct device *dev, enum sensor_channel chan,
                               struct sensor_value *val)
{
    struct daq_ntc_data *data = dev->data;

    if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
        return -ENOTSUP;
    }

    return sensor_value_from_float(val, data->temp);
}

static DEVICE_API(sensor, daq_ntc_api) = {
    .sample_fetch = daq_ntc_sample_fetch,
    .channel_get = daq_ntc_channel_get,
};

static int daq_ntc_init(const struct device *dev)
{
    const struct daq_ntc_config *config = dev->config;

    if (!device_is_ready(config->daq)) {
        LOG_ERR("DAQ device not ready");
        return -ENODEV;
    }

    return 0;
}

#define DAQ_NTC_CHANNEL_NODE(n) DT_INST_PHANDLE(n, daq_channel)

#define DAQ_NTC_INIT(n) \
    static const struct daq_ntc_config daq_ntc_config_##n = { \
        .daq = DEVICE_DT_GET(DT_PARENT(DAQ_NTC_CHANNEL_NODE(n))), \
        .pullup_ohm = DT_INST_PROP(n, pullup_ohm), \
        .nominal_ohm = DT_INST_PROP(n, nominal_ohm), \
        .beta = DT_INST_PROP(n, beta), \
        .channel = DT_NODE_CHILD_IDX(DAQ_NTC_CHANNEL_NODE(n)), \
    }; \
\
    static struct daq_ntc_data daq_ntc_data_##n; \
\
    SENSOR_DEVICE_DT_INST_DEFINE(n, daq_ntc_init, NULL, &daq_ntc_data_##n, &daq_ntc_config_##n, \
                                 POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &daq_ntc_api);

DT_INST_FOREACH_STATUS_OKAY(DAQ_NTC_INIT)
