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
#include "debug.h"

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

float solar_current_offset;
float load_current_offset;

// for ADC and DMA
volatile uint16_t adc_readings[NUM_ADC_CH] = {0};
volatile uint32_t adc_filtered[NUM_ADC_CH] = {0};
volatile AdcAlert adc_alerts_upper[NUM_ADC_CH] = {0};
volatile AdcAlert adc_alerts_lower[NUM_ADC_CH] = {0};

extern DeviceStatus dev_stat;

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

void adc_update_value(unsigned int pos)
{
    // low pass filter with filter constant c = 1/(2^ADC_FILTER_CONST)
    // y(n) = c * x(n) + (c - 1) * y(n-1)
    // see also here: http://techteach.no/simview/lowpass_filter/doc/filter_algorithm.pdf

#if FEATURE_PWM_SWITCH == 1
    if (pos == ADC_POS_V_SOLAR || pos == ADC_POS_I_SOLAR) {
        // only read input voltage and current when switch is on or permanently off
        if (pwm_switch.signal_high() || pwm_switch.active() == false) {
            adc_filtered[pos] += (uint32_t)adc_readings[pos] - (adc_filtered[pos] >> ADC_FILTER_CONST);
        }
    }
    else
#endif
    {
        // adc_readings: 12-bit ADC values left-aligned in uint16_t
        adc_filtered[pos] += (uint32_t)adc_readings[pos] - (adc_filtered[pos] >> ADC_FILTER_CONST);
    }

    // check upper alerts
    adc_alerts_upper[pos].debounce_ms++;
    if (adc_alerts_upper[pos].callback != NULL &&
        adc_readings[pos] > adc_alerts_upper[pos].limit)
    {
        if (adc_alerts_upper[pos].debounce_ms > 1) {
            // create function pointer and call function
            void (*alert)(void) = reinterpret_cast<void(*)()>(adc_alerts_upper[pos].callback);
            alert();
        }
    }
    else if (adc_alerts_upper[pos].debounce_ms > 0) {
        // reset debounce ms counter only if already close to triggering to allow setting negative
        // values to specify a one-time inhibit delay
        adc_alerts_upper[pos].debounce_ms = 0;
    }

    // same for lower alerts
    adc_alerts_lower[pos].debounce_ms++;
    if (adc_alerts_lower[pos].callback != NULL &&
        adc_readings[pos] < adc_alerts_lower[pos].limit)
    {
        if (adc_alerts_lower[pos].debounce_ms > 1) {
            void (*alert)(void) = reinterpret_cast<void(*)()>(adc_alerts_lower[pos].callback);
            alert();
        }
    }
    else if (adc_alerts_lower[pos].debounce_ms > 0) {
        adc_alerts_lower[pos].debounce_ms = 0;
    }
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

    // calculate lower voltage first, as it is needed for PWM terminal voltage calculation
    lv_terminal.voltage =
        (float)(((adc_filtered[ADC_POS_V_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_BAT / 1000.0;
    load_terminal.voltage = lv_terminal.voltage;

#if FEATURE_DCDC_CONVERTER
    hv_terminal.voltage =
        (float)(((adc_filtered[ADC_POS_V_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_SOLAR / 1000.0;
    dcdc_lv_port.voltage = lv_terminal.voltage;
#endif
#if FEATURE_PWM_SWITCH
    pwm_terminal.voltage = lv_terminal.voltage - vcc * ADC_OFFSET_V_SOLAR / 1000.0 -
        (float)(((adc_filtered[ADC_POS_V_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_V_SOLAR / 1000.0;
    pwm_port_int.voltage = lv_terminal.voltage;
#endif

    load_terminal.current =
        (float)(((adc_filtered[ADC_POS_I_LOAD] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_LOAD / 1000.0 + load_current_offset;

#if FEATURE_PWM_SWITCH
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power calculation
    pwm_port_int.current = pwm_switch.get_duty_cycle() * (
        (float)(((adc_filtered[ADC_POS_I_SOLAR] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_SOLAR / 1000.0 + solar_current_offset);
    pwm_terminal.current = -pwm_port_int.current;
    lv_terminal.current = pwm_port_int.current - load_terminal.current;

    pwm_port_int.power = pwm_port_int.voltage * pwm_port_int.current;
    pwm_terminal.power = pwm_terminal.voltage * pwm_terminal.current;
#endif
#if FEATURE_DCDC_CONVERTER
    dcdc_lv_port.current =
        (float)(((adc_filtered[ADC_POS_I_DCDC] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096) *
        ADC_GAIN_I_DCDC / 1000.0 + solar_current_offset;
    lv_terminal.current = dcdc_lv_port.current - load_terminal.current;
    hv_terminal.current = -dcdc_lv_port.current * lv_terminal.voltage / hv_terminal.voltage;

    dcdc_lv_port.power  = dcdc_lv_port.voltage * dcdc_lv_port.current;
    hv_terminal.power   = hv_terminal.voltage * hv_terminal.current;
#endif
    lv_terminal.power   = lv_terminal.voltage * lv_terminal.current;
    load_terminal.power = load_terminal.voltage * load_terminal.current;

    /** \todo Improved (faster) temperature calculation:
       https://www.embeddedrelated.com/showarticle/91.php
    */

#if defined PIN_ADC_TEMP_BAT || defined PIN_ADC_TEMP_FETS
    float v_temp, rts;
#endif

#ifdef PIN_ADC_TEMP_BAT
    // battery temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_BAT] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = NTC_SERIES_RESISTOR * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    // Temperature calculation using Beta equation for 10k thermistor
    // (25°C reference temperature for Beta equation assumed)
    float bat_temp = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C

    if (bat_temp > -50) {
        // external sensor connected: take measured value
        charger.bat_temperature = bat_temp;
        charger.ext_temp_sensor = true;
    }
    else {
        // no external sensor: assume typical room temperature
        charger.bat_temperature = 25;
        charger.ext_temp_sensor = false;
    }
#endif

#ifdef PIN_ADC_TEMP_FETS
    // MOSFET temperature calculation
    v_temp = ((adc_filtered[ADC_POS_TEMP_FETS] >> (4 + ADC_FILTER_CONST)) * vcc) / 4096;  // voltage read by ADC (mV)
    rts = 10000 * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)
    dcdc.temp_mosfets = 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C
#endif

    // internal MCU temperature
    uint16_t adcval = (adc_filtered[ADC_POS_TEMP_MCU] >> (4 + ADC_FILTER_CONST)) * vcc / VREFINT_VALUE;
    dev_stat.internal_temp = (TSENSE_CAL2_VALUE - TSENSE_CAL1_VALUE) /
        (TSENSE_CAL2 - TSENSE_CAL1) * (adcval - TSENSE_CAL1) + TSENSE_CAL1_VALUE;

    if (dev_stat.internal_temp > 80) {
        dev_stat.set_error(ERR_INT_OVERTEMP);
    }
    else if (dev_stat.internal_temp < 70 && (dev_stat.has_error(ERR_INT_OVERTEMP))) {
        // remove error flag with hysteresis of 10°C
        dev_stat.clear_error(ERR_INT_OVERTEMP);
    }
    // else: keep previous setting
}

void high_voltage_alert()
{
    // disable any sort of input
#if FEATURE_DCDC_CONVERTER
    dcdc.emergency_stop();
#endif
#if FEATURE_PWM_SWITCH
    pwm_switch.emergency_stop();
#endif
    // do not use enter_state function, as we don't want to wait entire recharge delay
    charger.state = CHG_STATE_IDLE;

    dev_stat.set_error(ERR_BAT_OVERVOLTAGE);

    print_error("High voltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS_V_BAT], adc_alerts_upper[ADC_POS_V_BAT].limit);
}

void low_voltage_alert()
{
    // disable load output
    load.emergency_stop(LOAD_STATE_OFF_OVERCURRENT);
    charger.port->neg_current_limit = 0;

    dev_stat.set_error(ERR_BAT_UNDERVOLTAGE);

    print_error("Low voltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS_V_BAT], adc_alerts_lower[ADC_POS_V_BAT].limit);
}

void adc_upper_alert_inhibit(int adc_pos, int timeout_ms)
{
    // set negative value so that we get a final debouncing of this timeout + the original
    // delay in the alert function (currently only waiting for 2 samples = 2 ms)
    adc_alerts_upper[adc_pos].debounce_ms = -timeout_ms;
}

void adc_set_lv_alerts(float upper, float lower)
{
    int vcc = VREFINT_VALUE * VREFINT_CAL /
        (adc_filtered[ADC_POS_VREF_MCU] >> (4 + ADC_FILTER_CONST));

    // LV side (battery) overvoltage alert
    adc_alerts_upper[ADC_POS_V_BAT].limit =
        (uint16_t)((upper * 1000 / (ADC_GAIN_V_BAT) * 4096.0 / vcc)) << 4;
    adc_alerts_upper[ADC_POS_V_BAT].callback = (void *) &high_voltage_alert;

    // LV side (battery) undervoltage alert
    adc_alerts_lower[ADC_POS_V_BAT].limit =
        (uint16_t)((lower * 1000 / (ADC_GAIN_V_BAT) * 4096.0 / vcc)) << 4;
    adc_alerts_lower[ADC_POS_V_BAT].callback = (void *) &low_voltage_alert;
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
        for (unsigned int i = 0; i < NUM_ADC_CH; i++) {
            adc_update_value(i);
        }
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
