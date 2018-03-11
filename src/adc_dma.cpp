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
#include "config.h"
#include "data_objects.h"

// for DMA and ADC configuration using ST HAL (see below)
#include "pinmap.h"
extern const PinMap PinMap_ADC[];
extern const PinMap PinMap_ADC_Internal[];


// for ADC and DMA
#define NUM_ADC_CH 7
volatile uint16_t adc_readings[NUM_ADC_CH] = {0,0,0,0,0,0,0};
volatile uint32_t adc_filtered[NUM_ADC_CH] = {0,0,0,0,0,0,0};
#define ADC_FILTER_CONST 5          // filter multiplier = 1/(2^ADC_FILTER_CONST)
volatile int num_adc_conversions;

bool new_reading_available = false;

extern float solar_voltage;
extern float battery_voltage;
extern float dcdc_current;
extern float dcdc_current_offset;
extern float load_current;     
extern float load_current_offset;
extern float temp_mosfets;
extern float temp_battery;

extern Serial serial;
extern CalibrationData cal;

//----------------------------------------------------------------------------
void update_measurements()
{
    int v_temp, rts;

    // reference voltage of 2.5 V at PIN_V_REF
    int vcc = 2500 * 4096 / (adc_filtered[2] >> (4 + ADC_FILTER_CONST));        // mV

    battery_voltage = 
        (float)(((adc_filtered[3] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        adc_channels[3].multiplier / 1000.0;

    solar_voltage = 
        (float)(((adc_filtered[4] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        adc_channels[4].multiplier / 1000.0;

    load_current = 
        (float)(((adc_filtered[5] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        adc_channels[5].multiplier / 1000.0 + load_current_offset;

    dcdc_current = 
        (float)(((adc_filtered[6] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        adc_channels[6].multiplier / 1000.0 + dcdc_current_offset;

    // battery temperature calculation
    v_temp = ((adc_filtered[0] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    temp_battery = 1.0/(1.0/(273.15+25) + 1.0/cal.thermistor_beta_value*log(rts/10000.0)) - 273.15; // °C
    
    // MOSFET temperature calculation
    v_temp = ((adc_filtered[1] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)
    temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/cal.thermistor_beta_value*log(rts/10000.0)) - 273.15; // °C
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
        for (int i = 0; i < NUM_ADC_CH; i++) {
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
    }
  */  
}


// ToDo: Change to more clean function with direct register access without using ST HAL
void setup_adc()
{
    uint32_t function = (uint32_t)NC;
    analogin_t _adc;
    analogin_t* obj = &_adc;
    static bool adc_calibrated = false;

    for (unsigned int i = 0; i < sizeof(adc_channels)/sizeof(AnalogValue_t); i++) {

        obj->pin = adc_channels[i].pin;
    
        // Get the peripheral name from the pin and assign it to the object
        obj->handle.Instance = (ADC_TypeDef *)pinmap_peripheral(obj->pin, PinMap_ADC);
        // Get the functions (adc channel) from the pin and assign it to the object
        function = pinmap_function(obj->pin, PinMap_ADC);
        // Configure GPIO
        pinmap_pinout(obj->pin, PinMap_ADC);

        MBED_ASSERT(obj->handle.Instance != (ADC_TypeDef *)NC);
        MBED_ASSERT(function != (uint32_t)NC);

        obj->channel = STM_PIN_CHANNEL(function);

        // Configure ADC object structures
        obj->handle.State = HAL_ADC_STATE_RESET;
        obj->handle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
        obj->handle.Init.Resolution            = ADC_RESOLUTION_12B;
        obj->handle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
        obj->handle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
        obj->handle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
        obj->handle.Init.LowPowerAutoWait      = DISABLE;
        obj->handle.Init.LowPowerAutoPowerOff  = DISABLE;
        obj->handle.Init.ContinuousConvMode    = DISABLE;
        obj->handle.Init.DiscontinuousConvMode = DISABLE;
        obj->handle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
        obj->handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
        obj->handle.Init.DMAContinuousRequests = DISABLE;
        obj->handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;

        __HAL_RCC_ADC1_CLK_ENABLE();

        if (HAL_ADC_Init(&obj->handle) != HAL_OK) {
            error("Cannot initialize ADC");
        }

        // ADC calibration is done only once
        if (!adc_calibrated) {
            adc_calibrated = true;
            HAL_ADCEx_Calibration_Start(&obj->handle);
        }

        ADC_ChannelConfTypeDef sConfig = {0};

        // Configure ADC channel
        sConfig.Rank         = ADC_RANK_CHANNEL_NUMBER;
        sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;

        // Clear all channels as it is not done in HAL_ADC_ConfigChannel()
        obj->handle.Instance->CHSELR = 0;

        HAL_ADC_ConfigChannel(&obj->handle, &sConfig);

        HAL_ADC_Start(&obj->handle); // Start conversion

        // Read out value at one time to finish ADC configuration
        if (HAL_ADC_PollForConversion(&obj->handle, 10) == HAL_OK) {
            HAL_ADC_GetValue(&obj->handle);
        }
    }

    // Configure ADC object structures
    obj->handle.State = HAL_ADC_STATE_RESET;
    obj->handle.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    obj->handle.Init.Resolution            = ADC_RESOLUTION_12B;
    obj->handle.Init.DataAlign             = ADC_DATAALIGN_LEFT;        // for exeponential moving average filter
    obj->handle.Init.ScanConvMode          = ADC_SCAN_DIRECTION_FORWARD;
    obj->handle.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    obj->handle.Init.LowPowerAutoWait      = DISABLE;
    obj->handle.Init.LowPowerAutoPowerOff  = DISABLE;
    obj->handle.Init.ContinuousConvMode    = DISABLE;
    obj->handle.Init.DiscontinuousConvMode = DISABLE;
    obj->handle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    //obj->handle.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T15_TRGO; //ADC_SOFTWARE_START;
    obj->handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    //obj->handle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING; //ADC_EXTERNALTRIGCONVEDGE_NONE;
    obj->handle.Init.DMAContinuousRequests = ENABLE; //DISABLE;
    obj->handle.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;

    if (HAL_ADC_Init(&obj->handle) != HAL_OK) {
        error("Cannot initialize ADC");
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
    ADC1->SMPR = ADC_SMPR_SMP_1;

    // Select ADC channels
    ADC1->CHSELR = 
        ADC_CHSELR_CHSEL0 |     /* PIN_TEMP_BAT PA_0 */
        ADC_CHSELR_CHSEL1 |     /* PIN_TEMP_INT PA_1 */
        ADC_CHSELR_CHSEL5 |     /* PIN_V_REF    PA_5 */
        ADC_CHSELR_CHSEL6 |     /* PIN_V_BAT    PA_6 */
        ADC_CHSELR_CHSEL7 |     /* PIN_V_SOLAR  PA_7 */
        ADC_CHSELR_CHSEL8 |     /* PIN_I_LOAD   PB_0 */
        ADC_CHSELR_CHSEL9;      /* PIN_I_DCDC   PB_1 */

    //ADC1->CFGR1 &= ~(ADC_CFGR1_EXTEN_0 | ADC_CFGR1_EXTEN_1);
    //ADC1->CFGR1 |= ADC_CFGR1_EXTEN_0;       // enable external trigger (e.g. timer)

    //ADC1->CFGR1 |= ADC_CFGR1_EXTSEL_2;      // set TIM15 as trigger source
}

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