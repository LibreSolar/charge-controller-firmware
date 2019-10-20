/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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
#endif

#include "main.h"

#include "adc_dma.h"
#include "pcb.h"        // contains defines for pins
#include <math.h>       // log for thermistor calculation

// factory calibration values for internal voltage reference and temperature sensor
// (see MCU datasheet, not RM)
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
#elif defined(UNIT_TEST)
    #define VREFINT_CAL (4096 * 1.224 / 3.0)   // VREFINT @3.0V/25°C
    #define VREFINT_VALUE 3000 // mV
    #define TSENSE_CAL1 (4096.0 * (670 - 161) / 3300)     // datasheet: slope 1.61 mV/°C
    #define TSENSE_CAL2 (4096.0 * 670 / 3300)     // datasheet: 670 mV
    #define TSENSE_CAL1_VALUE 30.0   // temperature of first calibration point
    #define TSENSE_CAL2_VALUE 130.0  // temperature of second calibration point
#endif

#ifdef PIN_REF_I_DCDC
AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
#endif

#ifdef PIN_TEMP_INT_PD
DigitalInOut temp_pd(PIN_TEMP_INT_PD);
#endif

float solar_current_offset;
float load_current_offset;

// for ADC and DMA
volatile uint16_t adc_readings[NUM_ADC_CH] = {0};
volatile uint32_t adc_filtered[NUM_ADC_CH] = {0};
//volatile int num_adc_conversions;

extern LogData log_data;
extern float mcu_temp;

void calibrate_current_sensors()
{
    int vcc = VREFINT_VALUE * VREFINT_CAL /
        (adc_filtered[ADC_POS_VREF_MCU] >> (4 + ADC_FILTER_CONST));
#if FEATURE_PWM_SWITCH
    solar_current_offset = -(float)(((adc_filtered[ADC_POS_I_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) * ADC_GAIN_I_SOLAR / 1000.0;
#endif
#if FEATURE_DCDC_CONVERTER
    solar_current_offset = -(float)(((adc_filtered[ADC_POS_I_DCDC] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) * ADC_GAIN_I_DCDC / 1000.0;
#endif
    load_current_offset = -(float)(((adc_filtered[ADC_POS_I_LOAD] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) * ADC_GAIN_I_LOAD / 1000.0;
}

//----------------------------------------------------------------------------
void update_measurements()
{
    // reference voltage of 2.5 V at PIN_V_REF
    //int vcc = 2500 * 4096 / (adc_filtered[ADC_POS_V_REF] >> (4 + ADC_FILTER_CONST));

    // internal STM reference voltage
    int vcc = VREFINT_VALUE * VREFINT_CAL /
        (adc_filtered[ADC_POS_VREF_MCU] >> (4 + ADC_FILTER_CONST));

    // rely on LDO accuracy
    //int vcc = 3300;

    // calculate LS voltage first, as it is needed for HS voltage calculation for PWM charge controller
    lv_bus.voltage =
        (float)(((adc_filtered[ADC_POS_V_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_BAT / 1000.0;

    hv_bus.voltage =
        (float)(((adc_filtered[ADC_POS_V_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_SOLAR / 1000.0;
#ifdef ADC_OFFSET_V_SOLAR
    hv_bus.voltage = hv_bus.voltage + -(vcc * ADC_OFFSET_V_SOLAR / 1000.0 + hv_bus.voltage);
#endif

    load_terminal.current =
        (float)(((adc_filtered[ADC_POS_I_LOAD] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + load_current_offset;

#if FEATURE_PWM_SWITCH
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power calculation
    hv_terminal.current = - pwm_switch_get_duty_cycle() * (
        (float)(((adc_filtered[ADC_POS_I_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_SOLAR / 1000.0 + solar_current_offset);
    lv_terminal.current = -hv_terminal.current - load_terminal.current;
#endif
#if FEATURE_DCDC_CONVERTER
    dcdc_port_lv.current =
        (float)(((adc_filtered[ADC_POS_I_DCDC] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_DCDC / 1000.0 + solar_current_offset;
    lv_terminal.current = dcdc_port_lv.current - load_terminal.current;
    hv_terminal.current = -dcdc_port_lv.current * lv_bus.voltage / hv_bus.voltage;
    dcdc_port_hv.current = hv_terminal.current;

    dcdc_port_lv.power  = lv_bus.voltage * dcdc_port_lv.current;
    dcdc_port_hv.power  = hv_bus.voltage * dcdc_port_hv.current;
#endif

    hv_terminal.power   = hv_bus.voltage * hv_terminal.current;
    lv_terminal.power   = lv_bus.voltage * lv_terminal.current;
    load_terminal.power = lv_bus.voltage * load_terminal.current;

    /** \todo Improved (faster) temperature calculation:
       https://www.embeddedrelated.com/showarticle/91.php
    */

#if defined PIN_ADC_TEMP_BAT || defined PIN_ADC_TEMP_FETS
    float v_temp, rts;
#endif
    float bat_temp = 25.0;

#ifdef PIN_ADC_TEMP_BAT
    // battery temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = NTC_SERIES_RESISTOR * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    bat_temp = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif

    detect_battery_temperature(&charger, bat_temp);

#ifdef PIN_ADC_TEMP_FETS
    // MOSFET temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_FETS] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)
    dcdc.temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif

    // internal MCU temperature
    uint16_t adcval = (adc_filtered[ADC_POS_TEMP_MCU] >> (4 + ADC_FILTER_CONST)) * vcc / VREFINT_VALUE;
    mcu_temp = (TSENSE_CAL2_VALUE - TSENSE_CAL1_VALUE) / (TSENSE_CAL2 - TSENSE_CAL1) * (adcval - TSENSE_CAL1) + TSENSE_CAL1_VALUE;
    //printf("TS_CAL1:%d TS_CAL2:%d ADC:%d, temp_int:%f\n", TS_CAL1, TS_CAL2, adcval, meas->temp_int);
}

void detect_battery_temperature(Charger *charger, float bat_temp)
{
#ifdef PIN_TEMP_INT_PD

    // state machine for external sensor detection
    enum temp_sense_state {
        TSENSE_STATE_CHECK,
        TSENSE_STATE_CHECK_WAIT,
        TSENSE_STATE_MEASURE,
        TSENSE_STATE_MEASURE_WAIT
    };
    static temp_sense_state ts_state = TSENSE_STATE_CHECK;

    static int sensor_change_counter = 0;

    switch (ts_state) {
        case TSENSE_STATE_CHECK:
            if (bat_temp < -50) {
                charger->ext_temp_sensor = false;
                temp_pd.output();
                temp_pd = 0;
                ts_state = TSENSE_STATE_CHECK_WAIT;
            }
            else {
                charger->ext_temp_sensor = true;
                ts_state = TSENSE_STATE_MEASURE;
            }
            break;
        case TSENSE_STATE_CHECK_WAIT:
            sensor_change_counter++;
            if (sensor_change_counter > 5) {
                sensor_change_counter = 0;
                ts_state = TSENSE_STATE_MEASURE;
            }
            break;
        case TSENSE_STATE_MEASURE:
            charger->bat_temperature = charger->bat_temperature * 0.8 + bat_temp * 0.2;
            //printf("Battery temperature: %.2f (%s sensor)\n", bat_temp, (charger->ext_temp_sensor ? "external" : "internal"));
            temp_pd.input();
            ts_state = TSENSE_STATE_MEASURE_WAIT;
            break;
        case TSENSE_STATE_MEASURE_WAIT:
            sensor_change_counter++;
            if (sensor_change_counter > 10) {
                sensor_change_counter = 0;
                ts_state = TSENSE_STATE_CHECK;
            }
            break;
    }
#else
    if (bat_temp > -50) {
        charger->bat_temperature = bat_temp;
    }
#endif
}

#ifndef UNIT_TEST

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
    NVIC_SetPriority(DMA1_Channel1_IRQn, 2);
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
#ifdef CHARGER_TYPE_PWM
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            if (i == ADC_POS_V_SOLAR || i == ADC_POS_I_SOLAR) {
                // only read input voltage and current when switch is on or permanently off
                if (GPIOB->IDR & GPIO_PIN_1 || pwm_switch_enabled() == false) {
                    adc_filtered[i] += (uint32_t)adc_readings[i] - (adc_filtered[i] >> ADC_FILTER_CONST);
                }
            }
            else {
                adc_filtered[i] += (uint32_t)adc_readings[i] - (adc_filtered[i] >> ADC_FILTER_CONST);
            }
        }
#else
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            // adc_readings: 12-bit ADC values left-aligned in uint16_t
            adc_filtered[i] += (uint32_t)adc_readings[i] - (adc_filtered[i] >> ADC_FILTER_CONST);
        }
#endif
    }
    DMA1->IFCR |= 0x0FFFFFFF;       // clear all interrupt registers
}

void adc_setup()
{
#ifdef PIN_REF_I_DCDC
    ref_i_dcdc = 0.1;    // reference voltage for zero current (0.1 for buck, 0.9 for boost, 0.5 for bi-directional)
#endif

#ifdef PIN_V_SOLAR_EN
    DigitalOut solar_en(PIN_V_SOLAR_EN);
    solar_en = 1;
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

#endif // UNIT_TEST

#if defined(STM32F0)

void adc_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM15 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    // Set timer clock to 10 kHz
    TIM15->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM15->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM15->ARR = 10000 / freq_Hz - 1;

    // 2 = second-lowest priority of STM32L0/F0
    NVIC_SetPriority(TIM15_IRQn, 2);
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

void adc_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM6 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;

    // Set timer clock to 10 kHz
    TIM6->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM6->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM6->ARR = 10000 / freq_Hz - 1;

    // 2 = second-lowest priority of STM32L0/F0
    NVIC_SetPriority(TIM6_IRQn, 2);
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
