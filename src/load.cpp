/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#include "pcb.h"
#include "config.h"
#include "hardware.h"
#include "leds.h"
#include "device_status.h"
#include "debug.h"
#include "helper.h"

#ifdef __ZEPHYR__
#include <stm32l0xx_ll_system.h>
#endif

extern DeviceStatus dev_stat;

extern LoadOutput load;     // necessary to call emergency stop function

#if defined(__MBED__) && defined(PIN_I_LOAD_COMP) && PIN_LOAD_DIS == PB_2
static void lptim_init()
{
    // Enable peripheral clock of GPIOB
    RCC->IOPENR |= RCC_IOPENR_IOPBEN;

    // Enable LPTIM clock
    RCC->APB1ENR |= RCC_APB1ENR_LPTIM1EN;

    // Select alternate function mode on PB2 (first bit _1 = 1, second bit _0 = 0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE2)) | GPIO_MODER_MODE2_1;

    // Select AF2 (LPTIM_OUT) on PB2
    GPIOB->AFR[0] |= 0x2U << GPIO_AFRL_AFRL2_Pos;

    // Set prescaler to 32 (resulting in 1 MHz timer frequency)
    LPTIM1->CFGR |= 0x5U << LPTIM_CFGR_PRESC_Pos;

    // Enable trigger (rising edge)
    LPTIM1->CFGR |= LPTIM_CFGR_TRIGEN_0;

    // Select trigger 7 (COMP2_OUT)
    LPTIM1->CFGR |= 0x7U << LPTIM_CFGR_TRIGSEL_Pos;

    //LPTIM1->CFGR |= LPTIM_CFGR_WAVPOL;
    LPTIM1->CFGR |= LPTIM_CFGR_PRELOAD;

    // Glitch filter of 8 cycles
    LPTIM1->CFGR |= LPTIM_CFGR_TRGFLT_0 | LPTIM_CFGR_TRGFLT_1;

    // Enable set-once mode
    LPTIM1->CFGR |= LPTIM_CFGR_WAVE;

    // Enable timer (must be done *before* changing ARR or CMP, but *after*
    // changing CFGR)
    LPTIM1->CR |= LPTIM_CR_ENABLE;

    // Auto Reload Register
    LPTIM1->ARR = 1000;

    // Set load switch-off delay in microseconds
    // (actually takes approx. 4 us longer than this setting)
    LPTIM1->CMP = 10;

    // Continuous mode
    //LPTIM1->CR |= LPTIM_CR_CNTSTRT;
    //LPTIM1->CR |= LPTIM_CR_SNGSTRT;
}
#endif

static void short_circuit_comp_init()
{
#if defined(__MBED__) && defined(PIN_I_LOAD_COMP)
    MBED_ASSERT(PIN_I_LOAD_COMP == PB_4);

    // set GPIO pin to analog
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
    GPIOB->MODER &= ~(GPIO_MODER_MODE4);

    // enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // enable VREFINT buffer
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_ENBUFLP_VREFINT_COMP;

    // select PB4 as positive input
    COMP2->CSR |= COMP_INPUT_PLUS_IO2;

    // select VREFINT divider as negative input
    COMP2->CSR |= COMP_INPUT_MINUS_1_4VREFINT;

    // propagate comparator value to LPTIM input
    COMP2->CSR |= COMP_CSR_COMP2LPTIM1IN1;

    // normal polarity
    //COMP2->CSR |= COMP_CSR_COMP2POLARITY;

    // set high-speed mode (1.2us instead of 2.5us propagation delay, but 3.5uA instead of 0.5uA current consumption)
    //COMP2->CSR |= COMP_CSR_COMP2SPEED;

    // enable COMP2
    COMP2->CSR |= COMP_CSR_COMP2EN;

    // enable EXTI software interrupt / event
    EXTI->IMR |= EXTI_IMR_IM22;
    EXTI->EMR |= EXTI_EMR_EM22;
    EXTI->RTSR |= EXTI_RTSR_RT22;
    EXTI->FTSR |= EXTI_FTSR_FT22;
    EXTI->SWIER |= EXTI_SWIER_SWI22;

    // 1 = second-highest priority of STM32L0/F0
    NVIC_SetPriority(ADC1_COMP_IRQn, 1);
    NVIC_EnableIRQ(ADC1_COMP_IRQn);
#endif
}

#ifdef PIN_I_LOAD_COMP

extern "C" void ADC1_COMP_IRQHandler(void)
{
    // interrupt called because of COMP2?
    if (COMP2->CSR & COMP_CSR_COMP2VALUE) {
        // Load should be switched off by LPTIM trigger already. This interrupt
        // is mainly used to indicate the failure.
        load.stop(ERR_LOAD_SHORT_CIRCUIT);
    }

    // clear interrupt flag
    EXTI->PR |= EXTI_PR_PIF22;
}

#endif // PIN_I_LOAD_COMP


void LoadOutput::switch_set(bool status)
{
    pgood = status;
    leds_set(LED_LOAD, status);

#if defined(__MBED__) && defined(PIN_LOAD_EN)
    DigitalOut load_enable(PIN_LOAD_EN);
    if (status == true) {
        load_enable = 1;
    }
    else {
        load_enable = 0;
    }
#endif

#if defined(__MBED__) && defined(PIN_LOAD_DIS)
    DigitalOut load_disable(PIN_LOAD_DIS);
    if (status == true) {
        #ifdef PIN_I_LOAD_COMP
        lptim_init();
        #else
        load_disable = 0;
        #endif
    }
    else {
        load_disable = 1;
    }
#endif
    print_info("Load pgood = %d, state = %u\n", status, state);
}

void LoadOutput::usb_set(bool status)
{
    usb_pgood = status;

#if defined(__MBED__) && defined(PIN_USB_PWR_EN)
    DigitalOut usb_pwr_en(PIN_USB_PWR_EN);
    if (status == true) usb_pwr_en = 1;
    else usb_pwr_en = 0;
#endif
#if defined(__MBED__) && defined(PIN_USB_PWR_DIS)
    DigitalOut usb_pwr_dis(PIN_USB_PWR_DIS);
    if (status == true) usb_pwr_dis = 0;
    else usb_pwr_dis = 1;
#endif
    print_info("USB pgood = %d, state = %u, en = %d\n", status, usb_state, usb_enable);
}

LoadOutput::LoadOutput(PowerPort *pwr_port) :
    port(pwr_port)
{
    enable = true;
    usb_enable = true;
    state = LOAD_STATE_DISABLED;
    usb_state = LOAD_STATE_DISABLED;
    switch_set(false);
    usb_set(false);
    junction_temperature = 25;              // starting point: 25°C
    oc_recovery_delay = 5*60;               // default: 5 minutes
    lvd_recovery_delay = 60*60;             // default: 1 hour
    ov_hysteresis = 0.3;

    // analog comparator to detect short circuits and trigger immediate load switch-off
    short_circuit_comp_init();
}

void LoadOutput::usb_state_machine()
{
    // We don't have any overcurrent detection mechanism for USB and the buck converter IC should
    // reduce output power itself in case of failure. Battery overvoltage should also be handled by
    // the DC/DC up to a certain degree.
    // So, we don't use load failure states for the USB output. Only low SOC and exceeded
    // temperature limits force the USB output to be turned off.
    switch (usb_state) {
        case LOAD_STATE_DISABLED:
            if (usb_enable == true) {
                if (port->pos_current_limit > 0) {
                    usb_set(true);
                    usb_state = LOAD_STATE_ON;
                }
                else if (state == LOAD_STATE_OFF_LOW_SOC || state == LOAD_STATE_OFF_TEMPERATURE) {
                    usb_state = state;
                }
            }
            break;
        case LOAD_STATE_ON:
            // currently still same cut-off SOC limit as the load
            if (state == LOAD_STATE_OFF_LOW_SOC || state == LOAD_STATE_OFF_TEMPERATURE) {
                usb_set(false);
                usb_state = state;
            }
            else if (usb_enable == false) {
                usb_set(false);
                usb_state = LOAD_STATE_DISABLED;
            }
            break;
        case LOAD_STATE_OFF_LOW_SOC:
        case LOAD_STATE_OFF_TEMPERATURE:
            if (state == LOAD_STATE_ON) {
                // also go back to normal operation with the USB charging port
                usb_set(true);
                usb_state = LOAD_STATE_ON;
            }
            else if (state == LOAD_STATE_DISABLED) {
                usb_state = LOAD_STATE_DISABLED;
            }
            break;
    }
}

// deprecated function for legacy load state output
int LoadOutput::determine_load_state()
{
    if (pgood) {
        return LOAD_STATE_ON;
    }
    else if (dev_stat.has_error(ERR_LOAD_LOW_SOC)) {
        return LOAD_STATE_OFF_LOW_SOC;
    }
    else if (dev_stat.has_error(ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP)) {
        return LOAD_STATE_OFF_OVERCURRENT;
    }
    else if (dev_stat.has_error(ERR_LOAD_OVERVOLTAGE)) {
        return LOAD_STATE_OFF_OVERVOLTAGE;
    }
    else if (dev_stat.has_error(ERR_LOAD_SHORT_CIRCUIT)) {
        return LOAD_STATE_OFF_SHORT_CIRCUIT;
    }
    else if (enable == false) {
        return LOAD_STATE_DISABLED;
    }
    else {
        return LOAD_STATE_OFF_TEMPERATURE;
    }
}

// this function is called more often than the state machine
void LoadOutput::control()
{
    if (pgood) {

        // junction temperature calculation model for overcurrent detection
        junction_temperature = junction_temperature + (
                dev_stat.internal_temp - junction_temperature +
                port->current * port->current /
                (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) *
                (MOSFET_MAX_JUNCTION_TEMP - INTERNAL_MAX_REFERENCE_TEMP)
            ) / (MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY);

        if (junction_temperature > MOSFET_MAX_JUNCTION_TEMP
            || port->current > LOAD_CURRENT_MAX * 2)
        {
            dev_stat.set_error(ERR_LOAD_OVERCURRENT);
            oc_timestamp = uptime();
        }

        if (dev_stat.has_error(ERR_BAT_UNDERVOLTAGE)) {
            dev_stat.set_error(ERR_LOAD_LOW_SOC);
            lvd_timestamp = uptime();
        }

        // long-term overvoltage (overvoltage transients are detected as an ADC alert and switch
        // off the solar input instead of the load output)
        static int debounce_counter = 0;
        if (port->voltage > port->sink_voltage_max ||
            port->voltage > LOW_SIDE_VOLTAGE_MAX)
        {
            debounce_counter++;
            if (debounce_counter >= CONTROL_FREQUENCY) {      // waited 1s before setting the flag
                dev_stat.set_error(ERR_LOAD_OVERVOLTAGE);
            }
        }
        else {
            debounce_counter = 0;
        }

        if (dev_stat.has_error(ERR_LOAD_ANY)) {
            stop();
        }

        if (enable == false) {
            switch_set(false);
        }
    }
    else {
        // load is off: check if errors are resolved and if load can be switched on

        if (dev_stat.has_error(ERR_LOAD_LOW_SOC) &&
            !dev_stat.has_error(ERR_BAT_UNDERVOLTAGE) &&
            uptime() - lvd_timestamp > lvd_recovery_delay)
        {
            dev_stat.clear_error(ERR_LOAD_LOW_SOC);
        }

        if (dev_stat.has_error(ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP) &&
            uptime() - oc_timestamp > oc_recovery_delay)
        {
            dev_stat.clear_error(ERR_LOAD_OVERCURRENT);
            dev_stat.clear_error(ERR_LOAD_VOLTAGE_DIP);
        }

        if (dev_stat.has_error(ERR_LOAD_OVERVOLTAGE) &&
            port->voltage < (port->sink_voltage_max - ov_hysteresis) &&
            port->voltage < (LOW_SIDE_VOLTAGE_MAX - ov_hysteresis))
        {
            dev_stat.clear_error(ERR_LOAD_OVERVOLTAGE);
        }

        if (dev_stat.has_error(ERR_LOAD_SHORT_CIRCUIT) && enable == false) {
            // stay here until the charge controller is reset or load is manually switched off
            dev_stat.clear_error(ERR_LOAD_SHORT_CIRCUIT);
        }

        // finally switch on if all errors were resolved
        if (enable == true && !dev_stat.has_error(ERR_LOAD_ANY) && port->pos_current_limit > 0) {
            switch_set(true);
        }
    }

    // deprecated load state variable, will be removed in the future
    state = (int)determine_load_state();

    usb_state_machine();
}

void LoadOutput::stop(uint32_t error_flag)
{
    switch_set(false);
    dev_stat.set_error(error_flag);

    // flicker the load LED if failure was most probably caused by the user
    if (error_flag & (ERR_LOAD_OVERCURRENT | ERR_LOAD_VOLTAGE_DIP | ERR_LOAD_SHORT_CIRCUIT)) {
        leds_flicker(LED_LOAD);
        oc_timestamp = uptime();
    }
}
