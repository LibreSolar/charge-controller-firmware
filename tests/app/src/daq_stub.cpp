/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq_stub.h"

#include <libresolar/drivers/daq_emul.h>

#include "daq.h"

void prepare_adc_readings(AdcValues values)
{
    daq_emul_set_value(DAQ_DEV, DAQ_CH(v_high), values.solar_voltage);
    daq_emul_set_value(DAQ_DEV, DAQ_CH(v_low), values.battery_voltage);
    daq_emul_set_value(DAQ_DEV, DAQ_CH(i_dcdc), values.dcdc_current);
    daq_emul_set_value(DAQ_DEV, DAQ_CH(i_load), values.load_current);
}

void prepare_adc_filtered()
{
    daq_emul_preset_filtered(DAQ_DEV);
}
