/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libresolar/drivers/daq.h>
#include <libresolar/drivers/daq_emul.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/ztest.h>

#include <math.h>

#define DAQ_NODE DT_NODELABEL(daq)

static const struct device *const daq = DEVICE_DT_GET(DAQ_NODE);
static const struct device *const ntc = DEVICE_DT_GET(DT_NODELABEL(ntc));

static const int channel = DT_NODE_CHILD_IDX(DT_NODELABEL(daq_temp));

/* NTC resistance (beta = 3435, 10k at 25°C) */
#define R_NTC_25DEGC 10000.0F
#define R_NTC_50DEGC 4101.0F
#define R_PULLUP     10000.0F

static void set_ntc_resistance(float r_ntc)
{
    uint16_t raw = (uint16_t)(65536.0F * r_ntc / (r_ntc + R_PULLUP));

    zassert_ok(daq_emul_set_raw(daq, channel, raw));
    daq_emul_preset_filtered(daq);
}

static int read_temp(float *temp)
{
    struct sensor_value val;
    int err = sensor_sample_fetch(ntc);

    if (err != 0) {
        return err;
    }

    zassert_ok(sensor_channel_get(ntc, SENSOR_CHAN_AMBIENT_TEMP, &val));
    *temp = sensor_value_to_float(&val);

    return 0;
}

static void *daq_ntc_setup(void)
{
    zassert_true(device_is_ready(ntc));

    return NULL;
}

ZTEST(daq_ntc, test_nominal_temperature)
{
    float temp;

    set_ntc_resistance(R_NTC_25DEGC);
    zassert_ok(read_temp(&temp));
    zassert_within(temp, 25.0F, 0.1F);
}

ZTEST(daq_ntc, test_high_temperature)
{
    float temp;

    set_ntc_resistance(R_NTC_50DEGC);
    zassert_ok(read_temp(&temp));
    zassert_within(temp, 50.0F, 0.2F);
}

ZTEST(daq_ntc, test_not_connected)
{
    float temp;

    /*
     * NTC missing: input pulled up to reference voltage, but the maximum raw reading is still
     * one LSB below, resulting in an implausibly low temperature
     */
    zassert_ok(daq_emul_set_raw(daq, channel, UINT16_MAX));
    daq_emul_preset_filtered(daq);
    zassert_ok(read_temp(&temp));
    zassert_true(temp < -50.0F);
}

ZTEST(daq_ntc, test_shorted)
{
    float temp;

    zassert_ok(daq_emul_set_raw(daq, channel, 0));
    daq_emul_preset_filtered(daq);
    zassert_equal(read_temp(&temp), -ENODATA);
}

ZTEST(daq_ntc, test_unsupported_channel)
{
    struct sensor_value val;

    zassert_equal(sensor_sample_fetch_chan(ntc, SENSOR_CHAN_VOLTAGE), -ENOTSUP);
    zassert_equal(sensor_channel_get(ntc, SENSOR_CHAN_VOLTAGE, &val), -ENOTSUP);
}

ZTEST_SUITE(daq_ntc, NULL, daq_ntc_setup, NULL, NULL, NULL);
