/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "pcb.h"
#include "data_objects.h"
#include <math.h>     // log for thermistor calculation

// factory calibration values for internal voltage reference and temperature sensor (see MCU datasheet, not RM)
#if defined(STM32F0)
    const uint16_t VREFINT_CAL = *((uint16_t *)0x1FFFF7BA); // VREFINT @3.3V/30°C
    #define VREFINT_VALUE 3300 // mV
    const uint16_t TSENSE_CAL1 = *((uint16_t *)0x1FFFF7B8);
    const uint16_t TSENSE_CAL2 = *((uint16_t *)0x1FFFF7C2);
    #define TSENSE_CAL1_VALUE 30.0   // temperature of first calibration point
    #define TSENSE_CAL2_VALUE 110.0  // temperature of second calibration point
#elif defined(STM32L0)
    const uint16_t VREFINT_CAL = *((uint16_t *)0x1FF80078);   // VREFINT @3.0V/25°C
    #define VREFINT_VALUE 3000 // mV
    const uint16_t TSENSE_CAL1 = *((uint16_t *)0x1FF8007A);
    const uint16_t TSENSE_CAL2 = *((uint16_t *)0x1FF8007E);
    #define TSENSE_CAL1_VALUE 30.0   // temperature of first calibration point
    #define TSENSE_CAL2_VALUE 130.0  // temperature of second calibration point
#endif

#ifdef PIN_REF_I_DCDC
AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
#endif 

float dcdc_current_offset;
float load_current_offset;

// for ADC and DMA
volatile uint16_t adc_readings[NUM_ADC_CH] = {0};
volatile uint32_t adc_filtered[NUM_ADC_CH] = {0};
//volatile int num_adc_conversions;

#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)

extern Serial serial;

void calibrate_current_sensors(dcdc_t *dcdc, load_output_t *load)
{   
    dcdc_current_offset = -dcdc->ls_current;
    load_current_offset = -load->current;
}

//----------------------------------------------------------------------------
void update_measurements(dcdc_t *dcdc, battery_t *bat, load_output_t *load, dcdc_port_t *hs, dcdc_port_t *ls)
{
    int v_temp, rts;

    // reference voltage of 2.5 V at PIN_V_REF
    //int vcc = 2500 * 4096 / (adc_filtered[ADC_POS_V_REF] >> (4 + ADC_FILTER_CONST));

    // internal STM reference voltage
    int vcc = VREFINT_VALUE * VREFINT_CAL / (adc_filtered[ADC_POS_VREF_MCU] >> (4 + ADC_FILTER_CONST));
    
    // rely on LDO accuracy
    //int vcc = 3300;

    ls->voltage = 
        (float)(((adc_filtered[ADC_POS_V_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_BAT / 1000.0;

    hs->voltage = 
        (float)(((adc_filtered[ADC_POS_V_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_SOLAR / 1000.0;

    load->current = 
        (float)(((adc_filtered[ADC_POS_I_LOAD] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + load_current_offset;

    ls->current = 
        (float)(((adc_filtered[ADC_POS_I_DCDC] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + dcdc_current_offset;
    dcdc->ls_current = ls->current;

    hs->current = -ls->current * ls->voltage / hs->voltage;

    // ToDo: improved (faster) temperature calculation:
    // https://www.embeddedrelated.com/showarticle/91.php

#ifdef ADC_POS_TEMP_BAT
    // battery temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    bat->temp = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif

#ifdef ADC_POS_TEMP_FETS
    // MOSFET temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_FETS] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)
    dcdc->temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif

/*
    // internal MCU temperature
    uint16_t adcval = (adc_filtered[ADC_POS_TEMP_MCU] >> (4 + ADC_FILTER_CONST));
    meas->temp_int = (TSENSE_CAL2_VALUE - TSENSE_CAL1_VALUE) / (TSENSE_CAL2 - TSENSE_CAL1) * (adcval - TSENSE_CAL1) + TSENSE_CAL1_VALUE;
    //printf("TS_CAL1:%d TS_CAL2:%d ADC:%d, temp_int:%f\n", TS_CAL1, TS_CAL2, adcval, meas->temp_int);
*/
/*
    if (meas->battery_voltage > meas->battery_voltage_max) {
        meas->battery_voltage_max = meas->battery_voltage;
    }

    if (meas->solar_voltage > meas->solar_voltage_max) {
        meas->solar_voltage_max = meas->solar_voltage;
    }

    if (meas->dcdc_current > meas->dcdc_current_max) {
        meas->dcdc_current_max = meas->dcdc_current;
    }

    if (meas->load_current > meas->load_current_max) {
        meas->load_current_max = meas->load_current;
    }

    if (meas->temp_mosfets > meas->temp_mosfets_max) {
        meas->temp_mosfets_max = meas->temp_mosfets;
    }

    if (meas->temp_int > meas->temp_int_max) {
        meas->temp_int_max = meas->temp_int;
    }*/
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

    /* Configure NVIC for DMA (priority 16: default value according to startup code) */
    NVIC_SetPriority(DMA1_Channel1_IRQn, 16);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // Trigger ADC conversions
    ADC1->CR |= ADC_CR_ADSTART;
}

extern "C" void DMA1_Channel1_IRQHandler(void)
{
    if ((DMA1->ISR & DMA_ISR_TCIF1) != 0) // Test if transfer completed on DMA channel 1
    {
        // low pass filter with filter constant c = 1/16
        // y(n) = c * x(n) + (c - 1) * y(n-1)
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            // adc_readings: 12-bit ADC values left-aligned in uint16_t
            adc_filtered[i] += (uint32_t)adc_readings[i] - (adc_filtered[i] >> ADC_FILTER_CONST);
        }
        //num_adc_conversions++;
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
/*
    if (num_adc_conversions % 100 == 0) {
        serial.printf("Filtered: %d %d %d %d %d %d %d (num_conv: %d)\n", 
            adc_filtered[0] >> (4+ADC_FILTER_CONST), adc_filtered[1] >> (4+ADC_FILTER_CONST), adc_filtered[2] >> (4+ADC_FILTER_CONST), adc_filtered[3] >> (4+ADC_FILTER_CONST), 
            adc_filtered[4] >> (4+ADC_FILTER_CONST), adc_filtered[5] >> (4+ADC_FILTER_CONST), adc_filtered[6] >> (4+ADC_FILTER_CONST), num_adc_conversions);
    }*/
}

void adc_setup()
{
#ifdef PIN_REF_I_DCDC
    ref_i_dcdc = 0.5;    // reference voltage for zero current (0.1 for buck, 0.9 for boost, 0.5 for bi-directional)
#endif

    ADC_HandleTypeDef hadc;
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
        error("Cannot initialize ADC");
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
        error("Cannot initialize ADC");
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

#if defined(STM32F0)

void adc_timer_setup(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM15 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    // Set timer clock to 10 kHz
    TIM15->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM15->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM15->ARR = 10000 / freq_Hz - 1;

    NVIC_EnableIRQ(TIM15_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM15->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM15_IRQHandler(void)
{
    TIM15->SR &= ~(1 << 0);
    ADC1->CR |= ADC_CR_ADSTART;
}

#elif defined(STM32L0)

void adc_timer_setup(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    // Set timer clock to 10 kHz
    TIM6->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM6->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM6->ARR = 10000 / freq_Hz - 1;

    NVIC_EnableIRQ(TIM6_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM6->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM6_IRQHandler(void)
{
    TIM6->SR &= ~(1 << 0);
    ADC1->CR |= ADC_CR_ADSTART;
}

#endif
