/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBRESOLAR_DRIVERS_DAQ_STM32_H_
#define LIBRESOLAR_DRIVERS_DAQ_STM32_H_

#include <stm32_ll_adc.h>

/**
 * Initializer for the factory calibration values in struct daq_common_config
 *
 * The addresses and calibration conditions are defined in the LL header of each STM32 series.
 */
#define DAQ_STM32_CAL_CONFIG_INIT() \
    .vrefint_cal = VREFINT_CAL_ADDR, .vrefint_cal_mv = VREFINT_CAL_VREF, \
    .ts_cal1 = TEMPSENSOR_CAL1_ADDR, .ts_cal2 = TEMPSENSOR_CAL2_ADDR, \
    .ts_cal_mv = TEMPSENSOR_CAL_VREFANALOG, .ts_cal1_temp = TEMPSENSOR_CAL1_TEMP, \
    .ts_cal2_temp = TEMPSENSOR_CAL2_TEMP

#endif /* LIBRESOLAR_DRIVERS_DAQ_STM32_H_ */
