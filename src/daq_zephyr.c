/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "daq.h"

#if defined(__ZEPHYR__)

#include <stdint.h>
#include <inttypes.h>

#include <zephyr.h>
#include <drivers/adc.h>
#include <drivers/gpio.h>

#if defined(CONFIG_SOC_SERIES_STM32L0X)
#include <stm32l0xx_ll_system.h>
#include <stm32l0xx_ll_adc.h>
#include <stm32l0xx_ll_dac.h>
#include <stm32l0xx_ll_dma.h>
#include <stm32l0xx_ll_bus.h>
#elif defined(CONFIG_SOC_SERIES_STM32F0X)
#include <stm32f0xx_ll_system.h>
#include <stm32f0xx_ll_adc.h>
#include <stm32f0xx_ll_dac.h>
#include <stm32f0xx_ll_dma.h>
#include <stm32f0xx_ll_bus.h>
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
#include <stm32g4xx_ll_system.h>
#include <stm32g4xx_ll_dac.h>
#include <stm32g4xx_ll_dma.h>
#endif

#include "pcb.h"
#include "debug.h"

#if !defined(CONFIG_SOC_SERIES_STM32F0X) && !defined(CONFIG_SOC_SERIES_STM32L0X)
// Rank and sequence definitions for STM32G4 from adc_stm32.c Zephyr driver

#define RANK(n)		LL_ADC_REG_RANK_##n
static const u32_t table_rank[] = {
	RANK(1),
	RANK(2),
	RANK(3),
	RANK(4),
	RANK(5),
	RANK(6),
	RANK(7),
	RANK(8),
	RANK(9),
	RANK(10),
	RANK(11),
	RANK(12),
	RANK(13),
	RANK(14),
	RANK(15),
	RANK(16),
};

#define SEQ_LEN(n)	LL_ADC_REG_SEQ_SCAN_ENABLE_##n##RANKS
static const u32_t table_seq_len[] = {
	LL_ADC_REG_SEQ_SCAN_DISABLE,
	SEQ_LEN(2),
	SEQ_LEN(3),
	SEQ_LEN(4),
	SEQ_LEN(5),
	SEQ_LEN(6),
	SEQ_LEN(7),
	SEQ_LEN(8),
	SEQ_LEN(9),
	SEQ_LEN(10),
	SEQ_LEN(11),
	SEQ_LEN(12),
	SEQ_LEN(13),
	SEQ_LEN(14),
	SEQ_LEN(15),
	SEQ_LEN(16),
};
#endif /* !STM32F0X && !STM32L0X */

// for ADC and DMA
extern uint16_t adc_readings[];

void adc_update_value(unsigned int pos);

static void dac_setup()
{
#if defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
	LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_1, LL_DAC_OUTPUT_BUFFER_ENABLE);
	LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_1);
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1, 4096 / 10);
#endif /* STM32F0X || STM32L0X */
}

static void adc_setup()
{
#ifdef DT_SWITCH_V_SOLAR_EN_GPIOS_CONTROLLER
    struct device *dev = device_get_binding(DT_SWITCH_V_SOLAR_EN_GPIOS_CONTROLLER);
    gpio_pin_configure(dev, DT_SWITCH_V_SOLAR_EN_GPIOS_PIN, GPIO_DIR_OUT);
    gpio_pin_write(dev, DT_SWITCH_V_SOLAR_EN_GPIOS_PIN, 1);
#endif

    struct device *dev_adc = device_get_binding(DT_ADC_1_NAME);
    if (dev_adc == 0) {
        printf("ADC device not found\n");
        return;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_INTERNAL,
#if defined(CONFIG_SOC_SERIES_STM32F0X)
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 240),
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 161),
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
        // ToDo: channel-specific acquisition time. Only internal reference voltage
        //       and temperature need very long sampling time.
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 248),
#endif
        .channel_id = LL_ADC_CHANNEL_0,
        .differential = 0
    };
    adc_channel_setup(dev_adc, &channel_cfg);

    // The following configuration necessary for DMA is not yet possible with Zephyr driver,
    // so we need to use STM LL interface directly

    // enable internal reference voltage and temperature
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
        LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR);

#if defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
	LL_ADC_REG_SetSequencerChannels(ADC1, ADC_CHSEL);
#else
    for (int i = 0; i < NUM_ADC_1_CH; i++) {
        LL_ADC_REG_SetSequencerRanks(ADC1, table_rank[i], adc_1_sequence[i]);
    }
    LL_ADC_REG_SetSequencerLength(ADC1, table_seq_len[NUM_ADC_1_CH]);
#endif

    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_LEFT);
	LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);

    // Enable DMA transfer on ADC and circular mode
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);
}

static inline void adc_trigger_conversion(struct k_timer *timer_id)
{
    LL_ADC_REG_StartConversion(ADC1);
}

static void DMA1_Channel1_IRQHandler(void *args)
{
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0) // Test if transfer completed on DMA channel 1
    {
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            adc_update_value(i);
        }
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
}

static void dma_setup()
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
        LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),   // source address
        (uint32_t)(&(adc_readings[0])),     // destination address
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    // Configure the number of DMA transfers (data length in multiples of size per transfer)
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, NUM_ADC_CH);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);     // transfer error interrupt
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);     // transfer complete interrupt
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    // Configure NVIC for DMA (priority 2: second-lowest value for STM32L0/F0)
    IRQ_CONNECT(DMA1_Channel1_IRQn, 2, DMA1_Channel1_IRQHandler, 0, 0);
    irq_enable(DMA1_Channel1_IRQn);

    LL_ADC_REG_StartConversion(ADC1);
}

void daq_setup()
{
    static struct k_timer adc_trigger_timer;

    dac_setup();
    adc_setup();
    dma_setup();

    k_timer_init(&adc_trigger_timer, adc_trigger_conversion, NULL);
    k_timer_start(&adc_trigger_timer, K_MSEC(1), K_MSEC(1));        // 1 kHz

    k_sleep(500);      // wait for ADC to collect some measurement values
    daq_update();
    calibrate_current_sensors();
}

#endif // __ZEPHYR__
