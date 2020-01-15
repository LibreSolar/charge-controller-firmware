/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adc_dma.h"

#ifdef __ZEPHYR__

#include <zephyr.h>

#include "pcb.h"
#include "debug.h"

#include <stdint.h>
#include <inttypes.h>

#if defined(CONFIG_SOC_SERIES_STM32L0X)
#include <stm32l0xx_ll_system.h>
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
    //__HAL_RCC_DMA1_CLK_ENABLE();

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

    ADC_HandleTypeDef hadc = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    // Configure ADC object structures
    hadc.Instance                   = ADC1;
    hadc.State                      = HAL_ADC_STATE_RESET;
    hadc.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign             = ADC_DATAALIGN_LEFT;       // for exponential moving average filter
    hadc.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
    hadc.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait      = DISABLE;
    hadc.Init.LowPowerAutoPowerOff  = DISABLE;
    hadc.Init.ContinuousConvMode    = DISABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = ENABLE; //DISABLE;
    hadc.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&hadc) != HAL_OK) {
        print_error("Cannot initialize ADC");
    }

#if defined(STM32L0)
    HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED);
#else
    HAL_ADCEx_Calibration_Start(&hadc);
#endif

    // Configure ADC channel
    sConfig.Channel     = ADC_CHANNEL_0;            // can be any channel for initialization
    sConfig.Rank        = ADC_RANK_CHANNEL_NUMBER;

    // Clear all channels as it is not done in HAL_ADC_ConfigChannel()
    hadc.Instance->CHSELR = 0;

    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK) {
        print_error("Cannot initialize ADC");
    }

    HAL_ADC_Start(&hadc); // Start conversion

    // Read out value one time to finish ADC configuration
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
        HAL_ADC_GetValue(&hadc);
    }

    // ADC sampling time register
    // 000: 1.5 ADC clock cycles
    // 001: 7.5 ADC clock cycles
    // 010: 13.5 ADC clock cycles
    // 011: 28.5 ADC clock cycles
    // 100: 41.5 ADC clock cycles
    // 101: 55.5 ADC clock cycles
    // 110: 71.5 ADC clock cycles
    // 111: 239.5 ADC clock cycles
    //ADC1->SMPR = ADC_SMPR_SMP_1;      // for normal ADC OK
    ADC1->SMPR |= ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2;      // necessary for internal reference and temperature

    // Select ADC channels based on setup in config.h
    ADC1->CHSELR = ADC_CHSEL;

    // Enable internal voltage reference and temperature sensor
    // ToDo check sample rate
    ADC->CCR |= ADC_CCR_TSEN | ADC_CCR_VREFEN;
}

void adc_trigger_conversion(struct k_timer *timer_id)
{
    ADC1->CR |= ADC_CR_ADSTART;
}

#endif // __ZEPHYR__
