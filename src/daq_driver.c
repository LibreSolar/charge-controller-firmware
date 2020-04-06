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
#include <stm32g4xx_ll_adc.h>
#include <stm32g4xx_ll_dac.h>
#include <stm32g4xx_ll_dma.h>
#include <stm32g4xx_ll_bus.h>
#endif

#include "board.h"
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

static void vref_setup()
{
#ifdef CONFIG_SOC_SERIES_STM32G4X
    // output 2.048V at VREF+ pin (also used internally for ADC and DAC)
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
	LL_VREFBUF_SetVoltageScaling(LL_VREFBUF_VOLTAGE_SCALE0);
	LL_VREFBUF_Enable();
	LL_VREFBUF_DisableHIZ();
#endif
}

static void dac_setup()
{
#if defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
	LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_1, LL_DAC_OUTPUT_BUFFER_ENABLE);
	LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_1);
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1, 4096 / 10);
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_DAC1);
    /* DAC1 at PA4 for bi-directional DC/DC current measurement at 0.5 * VCC */
	LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_1, LL_DAC_OUTPUT_BUFFER_ENABLE);
	LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_1);
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_1, 4096 / 2);
    /* DAC1 at PA5 for uni-directional PWM and load current measurement at 0.1 * VCC */
	LL_DAC_SetOutputBuffer(DAC1, LL_DAC_CHANNEL_2, LL_DAC_OUTPUT_BUFFER_ENABLE);
	LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
    LL_DAC_ConvertData12RightAligned(DAC1, LL_DAC_CHANNEL_2, 4096 / 10);
#endif
}

static void adc_init(ADC_TypeDef *adc)
{
    LL_ADC_Disable(adc);
#if defined(CONFIG_SOC_SERIES_STM32F0X)
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);
    LL_ADC_SetClock(adc, LL_ADC_CLOCK_SYNC_PCLK_DIV4);
#elif defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
    LL_ADC_EnableInternalRegulator(adc);
    k_busy_wait(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(adc), LL_ADC_CLOCK_SYNC_PCLK_DIV4);
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC12);
    // Prepare for ADC calibration
    LL_ADC_DisableDeepPowerDown(adc);
    LL_ADC_EnableInternalRegulator(adc);
    k_busy_wait(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(adc), LL_ADC_CLOCK_SYNC_PCLK_DIV4);
#endif

#if defined(CONFIG_SOC_SERIES_STM32G4X)
    LL_ADC_StartCalibration(adc, LL_ADC_SINGLE_ENDED);
#elif defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)
    LL_ADC_StartCalibration(adc);
#endif
    while (LL_ADC_IsCalibrationOnGoing(adc)) {;}

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
#else // Others including CONFIG_SOC_SERIES_STM32G4X
    const uint32_t *adc_seq = (adc == ADC1) ? adc_1_sequence : adc_2_sequence;
    int num_ch = (adc == ADC1) ? NUM_ADC_1_CH : NUM_ADC_2_CH;
    for (int i = 0; i < num_ch; i++) {
        LL_ADC_REG_SetSequencerRanks(adc, table_rank[i], adc_seq[i]);
        LL_ADC_SetChannelSamplingTime(adc, adc_seq[i], LL_ADC_SAMPLINGTIME_47CYCLES_5);
    }
    LL_ADC_REG_SetSequencerLength(adc, table_seq_len[num_ch - 1]);
#endif // defined(CONFIG_SOC_SERIES_STM32F0X) || defined(CONFIG_SOC_SERIES_STM32L0X)

    LL_ADC_SetDataAlignment(adc, LL_ADC_DATA_ALIGN_LEFT);
    LL_ADC_SetResolution(adc, LL_ADC_RESOLUTION_12B);
    LL_ADC_REG_SetOverrun(adc, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    // Enable DMA transfer on ADC and circular mode
    LL_ADC_REG_SetDMATransfer(adc, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);

#if defined(CONFIG_SOC_SERIES_STM32G4X)
    if (adc == ADC2) {
        LL_ADC_REG_SetTriggerEdge(adc, LL_ADC_REG_TRIG_EXT_RISING);
        LL_ADC_REG_SetTriggerSource(adc, LL_ADC_REG_TRIG_EXT_TIM1_TRGO2);
    }
#endif
    LL_ADC_Enable(adc);
}

static void adc_setup()
{
#ifdef DT_SWITCH_V_SOLAR_GPIOS_CONTROLLER
    struct device *dev = device_get_binding(DT_SWITCH_V_SOLAR_GPIOS_CONTROLLER);
    gpio_pin_configure(dev, DT_SWITCH_V_SOLAR_GPIOS_PIN,
        DT_SWITCH_V_SOLAR_GPIOS_FLAGS | GPIO_OUTPUT_ACTIVE);
#endif

    adc_init(ADC1);

#if defined(CONFIG_SOC_SERIES_STM32G4X)
    adc_init(ADC2);
#endif
}

static inline void adc_trigger_conversion(struct k_timer *timer_id)
{
    LL_ADC_REG_StartConversion(ADC1);

    // ADC2 (if existing) is triggered by the PWM timer
}

static void DMA1_Channel1_IRQHandler(void *args)
{
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0) // Test if transfer completed on DMA channel 1
    {
        for (unsigned int i = 0; i < NUM_ADC_1_CH; i++) {
            adc_update_value(i);
        }
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
}

#if defined(CONFIG_SOC_SERIES_STM32G4X)
static void DMA2_Channel1_IRQHandler(void *args)
{
    if ((DMA2->ISR & DMA_ISR_TCIF1) != 0) { // Test if transfer completed on DMA channel 2
        for (unsigned int i = NUM_ADC_1_CH; i < NUM_ADC_1_CH + NUM_ADC_2_CH; i++) {
            adc_update_value(i);
        }
    }
    DMA2->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
}
#endif // CONFIG_SOC_SERIES_STM32G4X

// Assuming DMA1 is mapped to ADC1 and DMA2 mapped to ADC2.
static void dma_init(DMA_TypeDef *dma)
{
#if defined(CONFIG_SOC_SERIES_STM32G4X)
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMAMUX1);
    if (dma == DMA1) {
        LL_DMA_SetPeriphRequest(dma, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
    }
    else if (dma == DMA2) {
        LL_DMA_SetPeriphRequest(dma, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC2);
    }
#endif // CONFIG_SOC_SERIES_STM32G4X

    if (dma == DMA1) {
        LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

        LL_DMA_ConfigAddresses(dma, LL_DMA_CHANNEL_1,
            LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),   // source address
            (uint32_t)(&(adc_readings[0])),     // destination address
            LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

        // Configure the number of DMA transfers (data length in multiples of size per transfer)
        LL_DMA_SetDataLength(dma, LL_DMA_CHANNEL_1, NUM_ADC_1_CH);
    }
#if defined(CONFIG_SOC_SERIES_STM32G4X)
    else if (dma == DMA2) {
        LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

        // If DMA 2 is mapped to ADC 2
        LL_DMA_ConfigAddresses(dma, LL_DMA_CHANNEL_1,
            LL_ADC_DMA_GetRegAddr(ADC2, LL_ADC_DMA_REG_REGULAR_DATA),   // source address
            // destination address = position behind ADC_1 values
            (uint32_t)(&(adc_readings[NUM_ADC_1_CH])),
            LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

        // Configure the number of DMA transfers (data length in multiples of size per transfer)
        LL_DMA_SetDataLength(dma, LL_DMA_CHANNEL_1, NUM_ADC_2_CH);
    }
#endif // CONFIG_SOC_SERIES_STM32G4X

    LL_DMA_SetMemoryIncMode(dma, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetMemorySize(dma, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_HALFWORD);
    LL_DMA_SetPeriphSize(dma, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_HALFWORD);
    LL_DMA_EnableIT_TE(dma, LL_DMA_CHANNEL_1);     // transfer error interrupt
    LL_DMA_EnableIT_TC(dma, LL_DMA_CHANNEL_1);     // transfer complete interrupt
    LL_DMA_SetMode(dma, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);

    LL_DMA_EnableChannel(dma, LL_DMA_CHANNEL_1);

    // Configure NVIC for DMA (priority 2: second-lowest value for STM32L0/F0)
    if (dma == DMA1) {
        IRQ_CONNECT(DMA1_Channel1_IRQn, 2, DMA1_Channel1_IRQHandler, 0, 0);
        irq_enable(DMA1_Channel1_IRQn);
    }
#if defined(CONFIG_SOC_SERIES_STM32G4X)
    else if (dma == DMA2) {
        IRQ_CONNECT(DMA2_Channel1_IRQn, 2, DMA2_Channel1_IRQHandler, 0, 0);
        irq_enable(DMA2_Channel1_IRQn);
    }
#endif // CONFIG_SOC_SERIES_STM32G4X
}

static void dma_setup()
{
    dma_init(DMA1);
    LL_ADC_REG_StartConversion(ADC1);

#if defined(CONFIG_SOC_SERIES_STM32G4X)
    dma_init(DMA2);
    LL_ADC_REG_StartConversion(ADC2);
#endif // CONFIG_SOC_SERIES_STM32G4X
}

void daq_setup()
{
    static struct k_timer adc_trigger_timer;

    vref_setup();
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
