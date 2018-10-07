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

#ifndef UNIT_TEST

#include "mbed.h"
#include "config.h"
#include "data_objects.h"
//#include <math.h>     // log for thermistor calculation

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


float dcdc_current_offset;
float load_current_offset;

// for ADC and DMA
volatile uint16_t adc_readings[NUM_ADC_CH] = {0};
volatile uint32_t adc_filtered[NUM_ADC_CH] = {0};
volatile int num_adc_conversions;

#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)

bool new_reading_available = false;

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
    
    //int vcc = 3300;   // rely on LDO accuracy

    ls->voltage = 
        (float)(((adc_filtered[ADC_POS_V_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_BAT / 1000.0;
    //dcdc->ls_voltage = ls->voltage;

    hs->voltage = 
        (float)(((adc_filtered[ADC_POS_V_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_SOLAR / 1000.0;
    //dcdc->hs_voltage = hs->voltage;

    load->current = 
        (float)(((adc_filtered[ADC_POS_I_LOAD] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + load_current_offset;

    ls->current = 
        (float)(((adc_filtered[ADC_POS_I_DCDC] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + dcdc_current_offset;
    dcdc->ls_current = ls->current;

    hs->current = -ls->current * ls->voltage / hs->voltage;
    //meas->bat_current = meas->dcdc_current - meas->load_current;

#ifdef ADC_POS_TEMP_BAT
    // battery temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    bat->temp = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif
    // improved temperature calculation:
    // https://www.embeddedrelated.com/showarticle/91.php
/*
    // MOSFET temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_FETS] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)
    dcdc->temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
*/
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


void start_dma()
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
        num_adc_conversions++;
        if (num_adc_conversions % 100 == 0) {
            new_reading_available = true;
        }
        //DMA1->IFCR |= DMA_IFCR_CTCIF1;
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
/*
    if (num_adc_conversions % 100 == 0) {
        serial.printf("Filtered: %d %d %d %d %d %d %d (num_conv: %d)\n", 
            adc_filtered[0] >> (4+ADC_FILTER_CONST), adc_filtered[1] >> (4+ADC_FILTER_CONST), adc_filtered[2] >> (4+ADC_FILTER_CONST), adc_filtered[3] >> (4+ADC_FILTER_CONST), 
            adc_filtered[4] >> (4+ADC_FILTER_CONST), adc_filtered[5] >> (4+ADC_FILTER_CONST), adc_filtered[6] >> (4+ADC_FILTER_CONST), num_adc_conversions);
    }*/
}


// ToDo: Change to more clean function with direct register access without using ST HAL
void setup_adc()
{
    analogin_t _adc;
    analogin_t* obj = &_adc;

#if defined(STM32F0)
    // Enable HSI14 as ADC clock source.
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; // Enable the peripheral clock of the ADC
    RCC->CR2 |= RCC_CR2_HSI14ON; // Start HSI14 RC oscillator
    while ((RCC->CR2 & RCC_CR2_HSI14RDY) == 0) // Wait HSI14 is ready
    {
        // For robust implementation, add here time-out management
    }
    //ADC1->CFGR2 &= (~ADC_CFGR2_CKMODE); // Select HSI14 by writing 00 in CKMODE (reset value)
#elif defined(STM32L0)
    // ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE;  // reset value already
    RCC->CR |= RCC_CR_HSION; // Start HSI16 RC oscillator
    while ((RCC->CR & RCC_CR_HSIRDY) == 0)
    {
        // For robust implementation, add here time-out management
    }
#endif

    obj->pin = PA_0;     // start for one channel only
    obj->handle.Instance = (ADC_TypeDef *)ADC_1;

    // Configure ADC object structures
    obj->handle.State = HAL_ADC_STATE_RESET;
    obj->handle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    obj->handle.Init.Resolution            = ADC_RESOLUTION_12B;
    obj->handle.Init.DataAlign             = ADC_DATAALIGN_LEFT;       // for exeponential moving average filter
    obj->handle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
    obj->handle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    obj->handle.Init.LowPowerAutoWait      = DISABLE;
    obj->handle.Init.LowPowerAutoPowerOff  = DISABLE;
    obj->handle.Init.ContinuousConvMode    = DISABLE;
    obj->handle.Init.DiscontinuousConvMode = DISABLE;
    obj->handle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    obj->handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    obj->handle.Init.DMAContinuousRequests = ENABLE; //DISABLE;
    obj->handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&obj->handle) != HAL_OK) {
        error("Cannot initialize ADC");
    }

    // calibrate ADC
    if ((ADC1->CR & ADC_CR_ADEN) != 0) // Ensure that ADEN = 0
    {
        ADC1->CR |= ADC_CR_ADDIS; // Clear ADEN by setting ADDIS
    }
    while ((ADC1->CR & ADC_CR_ADEN) != 0)
    {
        // For robust implementation, add here time-out management
    }
    ADC1->CFGR1 &= ~ADC_CFGR1_DMAEN; // disable DMA during calibration
    ADC1->CR |= ADC_CR_ADCAL; // Launch the calibration by setting ADCAL
    while ((ADC1->CR & ADC_CR_ADCAL) != 0) // Wait until ADCAL=0
    {
        // For robust implementation, add here time-out management
    }

    ADC_ChannelConfTypeDef sConfig = {0};

    // Configure ADC channel
    sConfig.Rank         = ADC_RANK_CHANNEL_NUMBER;
#if defined(STM32F0)
    sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;
#endif

    // Clear all channels as it is not done in HAL_ADC_ConfigChannel()
    obj->handle.Instance->CHSELR = 0;

    HAL_ADC_ConfigChannel(&obj->handle, &sConfig);

    HAL_ADC_Start(&obj->handle); // Start conversion

    /*
    // Read out value one time to finish ADC configuration
    if (HAL_ADC_PollForConversion(&obj->handle, 10) == HAL_OK) {
        HAL_ADC_GetValue(&obj->handle);
    }
    */
    // Performs the AD conversion
    ADC1->CR |= ADC_CR_ADSTART; // Start the ADC conversion
    for (int i = 0 ; i < 4 ; i++)
    {
        while ((ADC1->ISR & ADC_ISR_EOC) == 0) // Wait end of conversion
        {
            // For robust implementation, add here time-out management
        }
        //ADC_Result[i] = ADC1->DR; // Store the ADC conversion result
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

/*
    // Select ADC channels
    ADC1->CHSELR = 
        ADC_CHSELR_CHSEL0 |     // PIN_TEMP_BAT PA_0
        ADC_CHSELR_CHSEL1 |     // PIN_TEMP_INT PA_1
        ADC_CHSELR_CHSEL5 |     // PIN_V_REF    PA_5
        ADC_CHSELR_CHSEL6 |     // PIN_V_BAT    PA_6
        ADC_CHSELR_CHSEL7 |     // PIN_V_SOLAR  PA_7
        ADC_CHSELR_CHSEL8 |     // PIN_I_LOAD   PB_0
        ADC_CHSELR_CHSEL9 |     // PIN_I_DCDC   PB_1
        ADC_CHSELR_CHSEL16;     // internal temperature ref
//        ADC_CHSELR_CHSEL17 |     // internal voltage ref
*/
    //ADC1->CFGR1 &= ~(ADC_CFGR1_EXTEN_0 | ADC_CFGR1_EXTEN_1);
    //ADC1->CFGR1 |= ADC_CFGR1_EXTEN_0;       // enable external trigger (e.g. timer)

    //ADC1->CFGR1 |= ADC_CFGR1_EXTSEL_2;      // set TIM15 as trigger source

    // Enable internal voltage reference and temperature sensor
    // ToDo check sample rate
    ADC->CCR |= ADC_CCR_TSEN | ADC_CCR_VREFEN;
}

#if defined(STM32F0)

void setup_adc_timer()
{
    // Enable peripheral clock of GPIOA and GPIOB
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    // timer clock: 48 MHz / 4800 = 10 kHz
    TIM15->PSC = 4799;

    // interrupt on timer update
    TIM15->DIER |= TIM_DIER_UIE;

    // Auto Reload Register
    TIM15->ARR = 10;        // 1 kHz sample frequency at 10 kHz clock

    //NVIC_SetPriority(DMA1_Channel1_IRQn, 16);
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

void setup_adc_timer()
{
    // Enable peripheral clock of GPIOA and GPIOB
    //RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    // timer clock: 48 MHz / 4800 = 10 kHz
    TIM6->PSC = 4799;

    // interrupt on timer update
    TIM6->DIER |= TIM_DIER_UIE;

    // Auto Reload Register
    TIM6->ARR = 10;        // 1 kHz sample frequency at 10 kHz clock

    //NVIC_SetPriority(DMA1_Channel1_IRQn, 16);
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

#endif /* UNIT_TEST */
