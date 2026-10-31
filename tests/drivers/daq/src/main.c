/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libresolar/drivers/daq.h>
#include <libresolar/drivers/daq_emul.h>

#include <zephyr/device.h>
#include <zephyr/ztest.h>

#define DAQ_NODE DT_NODELABEL(daq)

#define CH_V_IN    DAQ_CHANNEL(DAQ_NODE, v_in)
#define CH_I_BIDIR DAQ_CHANNEL(DAQ_NODE, i_bidir)
#define CH_TEMP    DAQ_CHANNEL(DAQ_NODE, temp_mcu)

#define I_BIDIR_OFFSET DT_PROP(DT_CHILD(DAQ_NODE, i_bidir), offset)

/* resolution of 12-bit readings for the v-in channel (gain 105.6 / 5.6) */
#define V_IN_LSB (3.3F / 4096 * 105.6F / 5.6F)

static const struct device *const dev = DEVICE_DT_GET(DAQ_NODE);

static int alert_count;
static int alert_channel;
static int sample_count;
static bool sample_filter_result;

static void alert_callback(const struct device *d, int channel)
{
    alert_count++;
    alert_channel = channel;
}

static bool sample_filter(const struct device *d, int channel)
{
    return sample_filter_result;
}

static void sample_callback(const struct device *d)
{
    sample_count++;
}

static void process_samples(int channel, int count)
{
    for (int i = 0; i < count; i++) {
        zassert_ok(daq_emul_process(dev, channel));
    }
}

static void *daq_setup(void)
{
    zassert_true(device_is_ready(dev), "DAQ device not ready");

    return NULL;
}

static void daq_before(void *fixture)
{
    ARG_UNUSED(fixture);

    /* restore offsets from devicetree, as they may have been changed by calibration */
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 0));
    zassert_ok(daq_emul_set_raw(dev, CH_I_BIDIR, I_BIDIR_OFFSET));
    zassert_ok(daq_emul_set_raw(dev, CH_TEMP, 0));
    daq_emul_preset_filtered(dev);
    zassert_ok(daq_calibrate_offset(dev, CH_V_IN));
    zassert_ok(daq_calibrate_offset(dev, CH_I_BIDIR));

    for (int ch = CH_V_IN; ch <= CH_TEMP; ch++) {
        zassert_ok(daq_set_alert(dev, ch, DAQ_ALERT_UPPER, 0, NULL));
        zassert_ok(daq_set_alert(dev, ch, DAQ_ALERT_LOWER, 0, NULL));
        zassert_ok(daq_inhibit_alert(dev, ch, DAQ_ALERT_UPPER, 0));
        zassert_ok(daq_inhibit_alert(dev, ch, DAQ_ALERT_LOWER, 0));
    }

    zassert_ok(daq_set_sample_filter(dev, NULL, 0));
    zassert_ok(daq_set_sample_callback(dev, NULL));

    alert_count = 0;
    alert_channel = -1;
    sample_count = 0;
}

ZTEST(daq, test_channel_index)
{
    zassert_equal(CH_V_IN, 0);
    zassert_equal(CH_I_BIDIR, 1);
    zassert_equal(CH_TEMP, 2);
}

ZTEST(daq, test_invalid_channel)
{
    zassert_equal(daq_get_raw(dev, -1), -EINVAL);
    zassert_equal(daq_get_raw(dev, 3), -EINVAL);
    zassert_equal(daq_calibrate_offset(dev, 3), -EINVAL);
    zassert_equal(daq_set_alert(dev, 3, DAQ_ALERT_UPPER, 1.0F, alert_callback), -EINVAL);
    zassert_equal(daq_inhibit_alert(dev, 3, DAQ_ALERT_UPPER, 10), -EINVAL);
    zassert_equal(daq_emul_process(dev, 3), -EINVAL);
}

ZTEST(daq, test_vref)
{
    zassert_equal(daq_get_vref_mv(dev), DAQ_EMUL_VREF_MV);
}

ZTEST(daq, test_raw_to_pin_voltage)
{
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 0));
    daq_emul_preset_filtered(dev);
    zassert_equal(daq_get_raw(dev, CH_V_IN), 0);
    zassert_within(daq_get_pin_voltage(dev, CH_V_IN), 0.0F, 0.001F);

    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 32767));
    daq_emul_preset_filtered(dev);
    zassert_equal(daq_get_raw(dev, CH_V_IN), 32767);
    zassert_within(daq_get_pin_voltage(dev, CH_V_IN), 1.65F, 0.01F);

    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 65535));
    daq_emul_preset_filtered(dev);
    zassert_equal(daq_get_raw(dev, CH_V_IN), 65535);
    zassert_within(daq_get_pin_voltage(dev, CH_V_IN), 3.3F, 0.01F);
}

ZTEST(daq, test_value_with_gain)
{
    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 12.0F));
    daq_emul_preset_filtered(dev);
    zassert_within(daq_get_value(dev, CH_V_IN), 12.0F, V_IN_LSB);
}

ZTEST(daq, test_value_with_offset)
{
    zassert_ok(daq_emul_set_value(dev, CH_I_BIDIR, 5.0F));
    daq_emul_preset_filtered(dev);
    zassert_within(daq_get_value(dev, CH_I_BIDIR), 5.0F, 0.05F);

    zassert_ok(daq_emul_set_value(dev, CH_I_BIDIR, -5.0F));
    daq_emul_preset_filtered(dev);
    zassert_within(daq_get_value(dev, CH_I_BIDIR), -5.0F, 0.05F);
}

ZTEST(daq, test_calibrate_offset)
{
    zassert_ok(daq_emul_set_raw(dev, CH_I_BIDIR, I_BIDIR_OFFSET + 500));
    daq_emul_preset_filtered(dev);
    zassert_true(daq_get_value(dev, CH_I_BIDIR) > 0.5F);

    zassert_ok(daq_calibrate_offset(dev, CH_I_BIDIR));
    zassert_within(daq_get_value(dev, CH_I_BIDIR), 0.0F, 0.001F);
}

ZTEST(daq, test_filter_convergence)
{
    daq_emul_reset_filtered(dev);
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 20000));
    zassert_ok(daq_emul_set_raw(dev, CH_I_BIDIR, 40000));

    /* first sample is attenuated by the filter */
    daq_emul_process_all(dev);
    zassert_true(daq_get_raw(dev, CH_V_IN) < 20000);

    /* collect samples until the filtered values have converged */
    for (int i = 0; i < 1000; i++) {
        daq_emul_process_all(dev);
    }
    zassert_equal(daq_get_raw(dev, CH_V_IN), 20000);
    zassert_equal(daq_get_raw(dev, CH_I_BIDIR), 40000);
}

ZTEST(daq, test_alert_upper_debounce)
{
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 15.0F, alert_callback));

    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 14.0F));
    process_samples(CH_V_IN, 5);
    zassert_equal(alert_count, 0);

    /* triggered at the second sample above the limit and then for each sample */
    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 16.0F));
    process_samples(CH_V_IN, 1);
    zassert_equal(alert_count, 0);
    process_samples(CH_V_IN, 1);
    zassert_equal(alert_count, 1);
    zassert_equal(alert_channel, CH_V_IN);
    process_samples(CH_V_IN, 1);
    zassert_equal(alert_count, 2);

    /* a single sample below the limit resets the debouncing */
    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 14.0F));
    process_samples(CH_V_IN, 1);
    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 16.0F));
    process_samples(CH_V_IN, 1);
    zassert_equal(alert_count, 2);
}

ZTEST(daq, test_alert_lower)
{
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_LOWER, 10.0F, alert_callback));

    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 11.0F));
    process_samples(CH_V_IN, 5);
    zassert_equal(alert_count, 0);

    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 9.0F));
    process_samples(CH_V_IN, 2);
    zassert_equal(alert_count, 1);
}

ZTEST(daq, test_alert_disabled)
{
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 15.0F, alert_callback));
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 15.0F, NULL));

    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 16.0F));
    process_samples(CH_V_IN, 5);
    zassert_equal(alert_count, 0);
}

ZTEST(daq, test_alert_inhibit)
{
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 15.0F, alert_callback));
    zassert_ok(daq_inhibit_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 10));

    /* inhibit time plus regular debouncing of 2 samples */
    zassert_ok(daq_emul_set_value(dev, CH_V_IN, 16.0F));
    process_samples(CH_V_IN, 11);
    zassert_equal(alert_count, 0);
    process_samples(CH_V_IN, 1);
    zassert_equal(alert_count, 1);
}

ZTEST(daq, test_alert_limit_clamped)
{
    /* limits beyond the range of the ADC are clamped instead of overflowing */
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 10000.0F, alert_callback));
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, UINT16_MAX - 1));
    process_samples(CH_V_IN, 3);
    zassert_equal(alert_count, 0);
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, UINT16_MAX));
    process_samples(CH_V_IN, 2);
    zassert_equal(alert_count, 1);

    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 0, NULL));
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_LOWER, -10000.0F, alert_callback));
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 1));
    process_samples(CH_V_IN, 3);
    zassert_equal(alert_count, 1);
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 0));
    process_samples(CH_V_IN, 2);
    zassert_equal(alert_count, 2);
}

ZTEST(daq, test_sample_filter)
{
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 1000));
    zassert_ok(daq_emul_set_raw(dev, CH_I_BIDIR, 1000));
    daq_emul_preset_filtered(dev);

    zassert_ok(daq_set_sample_filter(dev, sample_filter, BIT(CH_V_IN)));
    zassert_ok(daq_set_alert(dev, CH_V_IN, DAQ_ALERT_UPPER, 0.0F, alert_callback));

    /* rejected samples are not filtered, but alerts are still checked */
    sample_filter_result = false;
    zassert_ok(daq_emul_set_raw(dev, CH_V_IN, 2000));
    zassert_ok(daq_emul_set_raw(dev, CH_I_BIDIR, 2000));
    for (int i = 0; i < 100; i++) {
        daq_emul_process_all(dev);
    }
    zassert_equal(daq_get_raw(dev, CH_V_IN), 1000);
    zassert_equal(daq_get_raw(dev, CH_I_BIDIR), 2000);
    zassert_true(alert_count > 0);

    sample_filter_result = true;
    for (int i = 0; i < 1000; i++) {
        daq_emul_process_all(dev);
    }
    zassert_equal(daq_get_raw(dev, CH_V_IN), 2000);
}

ZTEST(daq, test_sample_callback)
{
    daq_emul_process_all(dev);
    zassert_equal(sample_count, 0);

    zassert_ok(daq_set_sample_callback(dev, sample_callback));
    daq_emul_process_all(dev);
    daq_emul_process_all(dev);
    zassert_equal(sample_count, 2);
}

ZTEST(daq, test_internal_temp)
{
    float temp;

    /* calibration values of the emulator: 0.76 V at 30 °C and 1.01 V at 130 °C */
    zassert_ok(daq_emul_set_raw(dev, CH_TEMP, (4096 * 760 / 3300) << 4));
    daq_emul_preset_filtered(dev);
    zassert_ok(daq_get_internal_temp(dev, &temp));
    zassert_within(temp, 30.0F, 0.5F);

    zassert_ok(daq_emul_set_raw(dev, CH_TEMP, (4096 * 1010 / 3300) << 4));
    daq_emul_preset_filtered(dev);
    zassert_ok(daq_get_internal_temp(dev, &temp));
    zassert_within(temp, 130.0F, 0.5F);
}

ZTEST_SUITE(daq, NULL, daq_setup, daq_before, NULL, NULL);
