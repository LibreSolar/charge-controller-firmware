/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adc_dma.h"

#include <math.h>       // log for thermistor calculation
#include <assert.h>

#ifdef __MBED__
#include "mbed.h"
#endif

#include "mcu.h"
#include "main.h"
#include "debug.h"
#include "pcb.h"        // contains defines for pins

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

#if defined(__MBED__) && defined(PIN_REF_I_DCDC)
AnalogOut ref_i_dcdc(PIN_REF_I_DCDC);
#endif


static float solar_current_offset;
static float load_current_offset;

// for ADC and DMA
static volatile uint16_t adc_readings[NUM_ADC_CH] = {};
static volatile uint32_t adc_filtered[NUM_ADC_CH] = {};
static volatile AdcAlert adc_alerts_upper[NUM_ADC_CH] = {};
static volatile AdcAlert adc_alerts_lower[NUM_ADC_CH] = {};

extern DeviceStatus dev_stat;

/**
 * Average value for ADC channel
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 */
static inline uint32_t adc_value(uint32_t channel)
{
    assert(channel < NUM_ADC_CH);
    return adc_filtered[channel] >> (4 + ADC_FILTER_CONST);
}

/**
 * Measured voltage for ADC channel after average
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 * @param vcc reference voltage in millivolts
 *
 * @return voltage in millivolts
 */
static inline float adc_voltage(uint32_t channel, int32_t vcc)
{
    return (adc_value(channel) * vcc) / 4096;
}
/**
 * Measured current/voltage for ADC channel after average and scaling
 * @param channel valid ADC channel pos ADC_POS_..., see adc_h.c
 * @param vcc reference voltage in millivolts
 *
 * @return scaled final value
 */
static inline float adc_scaled(uint32_t channel, int32_t vcc, const float gain)
{
    return adc_voltage(channel,vcc) * (gain/1000.0);
}

static inline float ntc_temp(uint32_t channel, int32_t vcc)
{
    /** \todo Improved (faster) temperature calculation:
       https://www.embeddedrelated.com/showarticle/91.php
    */

    float v_temp = adc_voltage(channel, vcc);  // voltage read by ADC (mV)
    float rts = NTC_SERIES_RESISTOR * v_temp / (vcc - v_temp); // resistance of NTC (Ohm)

    return 1.0/(1.0/(273.15+25) + 1.0/NTC_BETA_VALUE*log(rts/10000.0)) - 273.15; // °C

}


void calibrate_current_sensors()
{
    int vcc = VREFINT_VALUE * VREFINT_CAL / adc_value(ADC_POS_VREF_MCU);
#if FEATURE_PWM_SWITCH
    solar_current_offset = -adc_scaled(ADC_POS_I_SOLAR, vcc, ADC_GAIN_I_SOLAR);
#endif
#if FEATURE_DCDC_CONVERTER
    solar_current_offset = -adc_scaled(ADC_POS_I_DCDC, vcc, ADC_GAIN_I_DCDC);
#endif
    load_current_offset = -adc_scaled(ADC_POS_I_LOAD, vcc, ADC_GAIN_I_LOAD);
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
        adc_readings[pos] >= adc_alerts_upper[pos].limit)
    {
        if (adc_alerts_upper[pos].debounce_ms > 1) {
            // create function pointer and call function
            adc_alerts_upper[pos].callback();
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
        adc_readings[pos] <= adc_alerts_lower[pos].limit)
    {
        if (adc_alerts_lower[pos].debounce_ms > 1) {
            adc_alerts_lower[pos].callback();
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
    int vcc = VREFINT_VALUE * VREFINT_CAL / adc_value(ADC_POS_VREF_MCU);

    // rely on LDO accuracy
    //int vcc = 3300;

    // calculate lower voltage first, as it is needed for PWM terminal voltage calculation
    lv_terminal.voltage = adc_scaled(ADC_POS_V_BAT, vcc, ADC_GAIN_V_BAT);
    load_terminal.voltage = lv_terminal.voltage;

#if FEATURE_DCDC_CONVERTER
    hv_terminal.voltage = adc_scaled(ADC_POS_V_SOLAR, vcc, ADC_GAIN_V_SOLAR);
    dcdc_lv_port.voltage = lv_terminal.voltage;
#endif
#if FEATURE_PWM_SWITCH
    pwm_terminal.voltage = lv_terminal.voltage - vcc * (ADC_OFFSET_V_SOLAR / 1000.0) -
        adc_scaled(ADC_POS_V_SOLAR, vcc, ADC_GAIN_V_SOLAR);
    pwm_port_int.voltage = lv_terminal.voltage;
#endif

    load_terminal.current =
        adc_scaled(ADC_POS_I_LOAD, vcc, ADC_GAIN_I_LOAD) + load_current_offset;

#if FEATURE_PWM_SWITCH
    // current multiplied with PWM duty cycle for PWM charger to get avg current for correct power calculation
    pwm_port_int.current = pwm_switch.get_duty_cycle() * (
        adc_scaled(ADC_POS_I_SOLAR, vcc, ADC_GAIN_I_SOLAR) + solar_current_offset);
    pwm_terminal.current = -pwm_port_int.current;
    lv_terminal.current = pwm_port_int.current - load_terminal.current;

    pwm_port_int.power = pwm_port_int.voltage * pwm_port_int.current;
    pwm_terminal.power = pwm_terminal.voltage * pwm_terminal.current;
#endif
#if FEATURE_DCDC_CONVERTER
    dcdc_lv_port.current =
        adc_scaled(ADC_POS_I_DCDC, vcc, ADC_GAIN_I_DCDC) + solar_current_offset;
    lv_terminal.current = dcdc_lv_port.current - load_terminal.current;
    hv_terminal.current = -dcdc_lv_port.current * lv_terminal.voltage / hv_terminal.voltage;

    dcdc_lv_port.power  = dcdc_lv_port.voltage * dcdc_lv_port.current;
    hv_terminal.power   = hv_terminal.voltage * hv_terminal.current;
#endif
    lv_terminal.power   = lv_terminal.voltage * lv_terminal.current;
    load_terminal.power = load_terminal.voltage * load_terminal.current;

#ifdef PIN_ADC_TEMP_BAT
    // battery temperature calculation
    float bat_temp = ntc_temp(ADC_POS_TEMP_BAT, vcc);

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
    dcdc.temp_mosfets = ntc_temp(ADC_POS_TEMP_FETS, vcc);
#endif

    // internal MCU temperature
    uint16_t adcval = adc_value(ADC_POS_TEMP_MCU) * vcc / VREFINT_VALUE;
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
    // the battery undervoltage must have been caused by a load current peak
    load.stop(ERR_LOAD_VOLTAGE_DIP);

    print_error("Low voltage alert, ADC reading: %d limit: %d\n",
        adc_readings[ADC_POS_V_BAT], adc_alerts_lower[ADC_POS_V_BAT].limit);
}

void adc_upper_alert_inhibit(int adc_pos, int timeout_ms)
{
    // set negative value so that we get a final debouncing of this timeout + the original
    // delay in the alert function (currently only waiting for 2 samples = 2 ms)
    adc_alerts_upper[adc_pos].debounce_ms = -timeout_ms;
}

uint16_t adc_get_alert_limit(float scale, float limit)
{
    const float adclimit = (UINT16_MAX >> 4); // 12 bits ADC resolution
    const float limit_scaled = limit * scale;
    // even if we have a higher voltage limit, we must limit it
    // to the max value the ADC will be able to deliver
    return (limit_scaled > adclimit ? 0x0FFF : (uint16_t)(limit_scaled)) << 4;
    // shift 4 bits left to generate left aligned 16bit value
}

void adc_set_lv_alerts(float upper, float lower)
{
    int vcc = VREFINT_VALUE * VREFINT_CAL / adc_value(ADC_POS_VREF_MCU);
    float scale =  ((4096* 1000) / (ADC_GAIN_V_BAT)) / vcc;

    // LV side (battery) overvoltage alert
    adc_alerts_upper[ADC_POS_V_BAT].limit = adc_get_alert_limit(scale, upper);
    adc_alerts_upper[ADC_POS_V_BAT].callback = high_voltage_alert;

    // LV side (battery) undervoltage alert
    adc_alerts_lower[ADC_POS_V_BAT].limit = adc_get_alert_limit(scale, lower);
    adc_alerts_lower[ADC_POS_V_BAT].callback = low_voltage_alert;
}

#if defined(__MBED__) && !defined(UNIT_TEST)

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
#else

#include "../test/adc_dma_stub.h"

void prepare_adc_readings(AdcValues values)
{
    adc_readings[ADC_POS_VREF_MCU] = (uint16_t)(1.224 / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_SOLAR] = (uint16_t)((values.solar_voltage / (ADC_GAIN_V_SOLAR)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_BAT] = (uint16_t)((values.battery_voltage / (ADC_GAIN_V_BAT)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_DCDC] = (uint16_t)((values.dcdc_current / (ADC_GAIN_I_DCDC)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_LOAD] = (uint16_t)((values.load_current / (ADC_GAIN_I_LOAD)) / 3.3 * 4096) << 4;
}

void prepare_adc_filtered()
{
    // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = adc_readings[i] << ADC_FILTER_CONST;
    }
}

void clear_adc_filtered()
{
        // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = 0;
    }
}
uint32_t get_adc_filtered(uint32_t channel)
{
    return adc_value(channel);
}

#endif
