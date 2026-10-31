/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief DAQ driver for STM32 ADCs with a fixed channel sequence (STM32F0, STM32L0, STM32G0)
 *
 * The channels are selected with a bit mask and converted in ascending order of the channel
 * numbers. All channels have to be connected to ADC1.
 */

#define DT_DRV_COMPAT libresolar_stm32_daq

#include "daq_common.h"
#include "daq_stm32.h"

#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <soc.h>
#include <stm32_ll_adc.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_dac.h>
#include <stm32_ll_dma.h>

LOG_MODULE_DECLARE(daq, CONFIG_DAQ_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1, "Only one DAQ instance supported");

#define DAQ_NODE DT_DRV_INST(0)

#define ADC_CHSEL_FN(child) BIT(DT_IO_CHANNELS_INPUT(child)) |
#define ADC_CHSEL           (DT_FOREACH_CHILD(DAQ_NODE, ADC_CHSEL_FN) 0)

/* priority 2: second-lowest value for STM32L0/F0 */
#define DMA_IRQ_PRIORITY 2

struct daq_stm32_data
{
    struct daq_common_data common;
    struct k_timer trigger_timer;
};

DAQ_COMMON_DEFINE(DAQ_NODE);

static const struct daq_common_config daq_stm32_config = {
    DAQ_COMMON_CONFIG_INIT(DAQ_NODE),
    DAQ_STM32_CAL_CONFIG_INIT(),
};

static struct daq_stm32_data daq_stm32_data = {
    .common = { DAQ_COMMON_DATA_INIT() },
};

static void dac_setup(void)
{
    /* DAC1 at PA4 as reference voltage for current measurement amplifiers at 0.1 * VCC */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
    LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_1, LL_DAC_OUTPUT_BUFFER_ENABLE);
    LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_1);
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1, 4096 / 10);
}

static int adc_setup(ADC_TypeDef *adc)
{
#if defined(CONFIG_SOC_SERIES_STM32F0X)
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);
    LL_ADC_SetClock(adc, LL_ADC_CLOCK_SYNC_PCLK_DIV4);
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
    LL_ADC_EnableInternalRegulator(adc);
    k_busy_wait(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(adc), LL_ADC_CLOCK_SYNC_PCLK_DIV4);
#elif defined(CONFIG_SOC_SERIES_STM32G0X)
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC);
    LL_ADC_SetClock(adc, LL_ADC_CLOCK_SYNC_PCLK_DIV4);
    LL_ADC_EnableInternalRegulator(adc);
    k_busy_wait(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
#endif

    if (LL_ADC_IsEnabled(adc)) {
        LL_ADC_Disable(adc);
    }

    LL_ADC_StartCalibration(adc);
    while (LL_ADC_IsCalibrationOnGoing(adc)) {
    }

#if defined(CONFIG_SOC_SERIES_STM32G0X)
    // a few ADC clock cycles are required between end of calibration and enabling the ADC
    k_busy_wait(1);
#endif

    if (LL_ADC_IsActiveFlag_ADRDY(adc)) {
        LL_ADC_ClearFlag_ADRDY(adc);
    }

    // Enable internal reference voltage and temperature
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(adc),
                                   LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR);

#if defined(CONFIG_SOC_SERIES_STM32F0X)
    LL_ADC_REG_SetSequencerChannels(adc, ADC_CHSEL);
    LL_ADC_SetSamplingTimeCommonChannels(adc, LL_ADC_SAMPLINGTIME_239CYCLES_5);
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_ADC_REG_SetSequencerChannels(adc, ADC_CHSEL);
    LL_ADC_SetSamplingTimeCommonChannels(adc, LL_ADC_SAMPLINGTIME_160CYCLES_5);
#elif defined(CONFIG_SOC_SERIES_STM32G0X)
    LL_ADC_REG_SetSequencerConfigurable(adc, LL_ADC_REG_SEQ_FIXED);
    LL_ADC_ClearFlag_CCRDY(adc);
    LL_ADC_REG_SetSequencerChannels(adc, ADC_CHSEL);

    // channel configuration must be applied before starting conversions
    int timeout_us = 1000;
    while (!LL_ADC_IsActiveFlag_CCRDY(adc) && timeout_us-- > 0) {
        k_busy_wait(1);
    }
    if (timeout_us < 0) {
        LOG_ERR("ADC channel configuration timed out");
        return -ETIMEDOUT;
    }
    LL_ADC_ClearFlag_CCRDY(adc);

    LL_ADC_SetSamplingTimeCommonChannels(adc, LL_ADC_SAMPLINGTIME_COMMON_1,
                                         LL_ADC_SAMPLINGTIME_160CYCLES_5);
#endif

    LL_ADC_SetDataAlignment(adc, LL_ADC_DATA_ALIGN_LEFT);
    LL_ADC_SetResolution(adc, LL_ADC_RESOLUTION_12B);
    LL_ADC_REG_SetOverrun(adc, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    // Enable DMA transfer on ADC and circular mode
    LL_ADC_REG_SetDMATransfer(adc, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);

    LL_ADC_Enable(adc);

    return 0;
}

static void dma_isr(const void *arg)
{
    const struct device *dev = arg;
    const struct daq_common_config *config = dev->config;

    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0) { // transfer completed on DMA channel 1
        for (int i = 0; i < config->num_channels; i++) {
            daq_common_process(dev, i);
        }
        daq_common_sample_done(dev);
    }
    DMA1->IFCR |= 0x0FFFFFFF; // clear all interrupt flags
}

static void dma_setup(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_common_data *data = dev->data;

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

#if defined(CONFIG_SOC_SERIES_STM32G0X)
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
#endif

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
                           LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                           (uint32_t)data->buf, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    // Configure the number of DMA transfers (data length in multiples of size per transfer)
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, config->num_channels);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1); // transfer error interrupt
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1); // transfer complete interrupt
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    IRQ_CONNECT(DMA1_Channel1_IRQn, DMA_IRQ_PRIORITY, dma_isr, DEVICE_DT_INST_GET(0), 0);
    irq_enable(DMA1_Channel1_IRQn);
}

static void trigger_conversion(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    LL_ADC_REG_StartConversion(ADC1);
}

static int daq_stm32_init(const struct device *dev)
{
    const struct daq_common_config *config = dev->config;
    struct daq_stm32_data *data = dev->data;
    int err;

    // channels are converted and stored by the DMA in ascending order of the channel numbers
    for (int i = 0; i < config->num_channels; i++) {
        uint8_t idx = 0;

        for (int j = 0; j < config->num_channels; j++) {
            if (config->adc_channel[j] < config->adc_channel[i]) {
                idx++;
            }
            else if (config->adc_channel[j] == config->adc_channel[i] && j != i) {
                LOG_ERR("ADC channel %d used more than once", config->adc_channel[i]);
                return -EINVAL;
            }
        }
        data->common.channels[i].buf_idx = idx;
    }

    err = daq_common_init(dev);
    if (err != 0) {
        return err;
    }

    dac_setup();

    err = adc_setup(ADC1);
    if (err != 0) {
        return err;
    }

    dma_setup(dev);
    LL_ADC_REG_StartConversion(ADC1);

    k_timer_init(&data->trigger_timer, trigger_conversion, NULL);
    k_timer_start(&data->trigger_timer, K_MSEC(DAQ_SAMPLE_PERIOD_MS), K_MSEC(DAQ_SAMPLE_PERIOD_MS));

    return 0;
}

DEVICE_DT_INST_DEFINE(0, daq_stm32_init, NULL, &daq_stm32_data, &daq_stm32_config, POST_KERNEL,
                      CONFIG_DAQ_INIT_PRIORITY, NULL);
