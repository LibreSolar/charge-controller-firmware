/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adc_dma.h"

#ifdef __ZEPHYR__

#include <zephyr.h>
#include <drivers/adc.h>

#include "pcb.h"
#include "debug.h"

#include <stdint.h>
#include <inttypes.h>

#if defined(CONFIG_SOC_SERIES_STM32L0X)
#include <stm32l0xx_ll_system.h>
#include <stm32l0xx_ll_adc.h>
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
#include <stm32g4xx_ll_system.h>
#endif

// for ADC and DMA
extern uint16_t adc_readings[];

void adc_update_value(unsigned int pos);

void DMA1_Channel1_IRQHandler(void *args)
{
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0) // Test if transfer completed on DMA channel 1
    {
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            adc_update_value(i);
        }
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
}

void dma_setup()
{
    /* Enable the peripheral clock on DMA */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    /* Enable DMA transfer on ADC and circular mode */
    ADC1->CFGR1 |= ADC_CFGR1_DMAEN | ADC_CFGR1_DMACFG;

    /* Configure the peripheral data register address */
    DMA1_Channel1->CPAR = (uint32_t)(&(ADC1->DR));

    /* Configure the memory address */
    DMA1_Channel1->CMAR = (uint32_t)(&(adc_readings[0]));

    /* Configure the number of DMA tranfer to be performed on DMA channel 1 */
    DMA1_Channel1->CNDTR = NUM_ADC_CH;

    /* Configure increment, size, interrupts and circular mode */
    DMA1_Channel1->CCR =
        DMA_CCR_MINC |          /* memory increment mode enabled */
        DMA_CCR_MSIZE_0 |       /* memory size 16-bit */
        DMA_CCR_PSIZE_0 |       /* peripheral size 16-bit */
        DMA_CCR_TEIE |          /* transfer error interrupt enable */
        DMA_CCR_TCIE |          /* transfer complete interrupt enable */
        DMA_CCR_CIRC;           /* circular mode enable */
                                /* DIR = 0: read from peripheral */

    /* Enable DMA Channel 1 */
    DMA1_Channel1->CCR |= DMA_CCR_EN;

    /* Configure NVIC for DMA (priority 2: second-lowest value for STM32L0/F0) */
    IRQ_CONNECT(DMA1_Channel1_IRQn, 2, DMA1_Channel1_IRQHandler, 0, 0);
    irq_enable(DMA1_Channel1_IRQn);

    // Trigger ADC conversions
    ADC1->CR |= ADC_CR_ADSTART;
}

void adc_setup()
{
#ifdef PIN_REF_I_DCDC
    //ref_i_dcdc = 0.1;    // reference voltage for zero current (0.1 for buck, 0.9 for boost, 0.5 for bi-directional)
#endif

#ifdef PIN_V_SOLAR_EN
    //DigitalOut solar_en(PIN_V_SOLAR_EN);
    //solar_en = 1;
#endif

    struct device *dev_adc = device_get_binding(DT_ADC_1_NAME);
    if (dev_adc == 0) {
        printf("ADC device not found\n");
        return;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN_1,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 161),
        .channel_id = LL_ADC_CHANNEL_0,
        .differential = 0
    };
    adc_channel_setup(dev_adc, &channel_cfg);

    ADC_TypeDef *adc = ADC1;

    // enable internal reference voltage and temperature
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(adc),
        LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR);

#if defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
	LL_ADC_REG_SetSequencerChannels(adc, ADC_CHSEL);
#else
	LL_ADC_REG_SetSequencerRanks(adc, ADC_RANK_CHANNEL_NUMBER, ADC_CHSEL);
    LL_ADC_REG_SetSequencerLength(adc, LL_ADC_REG_SEQ_SCAN_DISABLE);
	//LL_ADC_REG_SetSequencerLength(adc, LL_ADC_REG_SEQ_SCAN_ENABLE_1RANKS);
#endif

    LL_ADC_SetDataAlignment(adc, LL_ADC_DATA_ALIGN_LEFT);
	LL_ADC_SetResolution(adc, LL_ADC_RESOLUTION_12B);
    LL_ADC_REG_SetOverrun(adc, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
}

void adc_trigger_conversion(struct k_timer *timer_id)
{
    LL_ADC_REG_StartConversion(ADC1);
}

#endif // __ZEPHYR__
