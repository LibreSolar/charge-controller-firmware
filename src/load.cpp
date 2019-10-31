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

#include "load.h"
#include "pcb.h"
#include "config.h"
#include "hardware.h"
#include "leds.h"
#include "log.h"
#include "debug.h"

volatile bool short_circuit = false;

extern LogData log_data;
extern float internal_temp;

#ifndef UNIT_TEST

#if defined(PIN_I_LOAD_COMP) && PIN_LOAD_DIS == PB_2
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
#ifdef PIN_I_LOAD_COMP
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
        leds_flicker(LED_LOAD);
        short_circuit = true;
    }

    // clear interrupt flag
    EXTI->PR |= EXTI_PR_PIF22;
}

#endif // PIN_I_LOAD_COMP


void LoadOutput::switch_set(bool enabled)
{
    leds_set(LED_LOAD, enabled);

#ifdef PIN_LOAD_EN
    DigitalOut load_enable(PIN_LOAD_EN);
    if (enabled) {
        load_enable = 1;
    }
    else {
        load_enable = 0;
    }
#endif

#ifdef PIN_LOAD_DIS
    DigitalOut load_disable(PIN_LOAD_DIS);
    #ifdef PIN_I_LOAD_COMP
    if (enabled) {
        lptim_init();
    }
    #else
    if (enabled) {
        load_disable = 0;
    }
    #endif
    else {
        load_disable = 1;
    }
    print_info("Load enabled = %d\n", enabled);
#endif
}

void LoadOutput::usb_set(bool enabled)
{
#ifdef PIN_USB_PWR_EN
    DigitalOut usb_pwr_en(PIN_USB_PWR_EN);
    if (enabled) usb_pwr_en = 1;
    else usb_pwr_en = 0;
#endif
#ifdef PIN_USB_PWR_DIS
    DigitalOut usb_pwr_dis(PIN_USB_PWR_DIS);
    if (enabled) usb_pwr_dis = 0;
    else usb_pwr_dis = 1;
#endif
}

#else /* UNIT_TEST */

// dummy functions
static void short_circuit_comp_init() {;}
void LoadOutput::switch_set(bool enabled) {;}
void LoadOutput::usb_set(bool enabled) {;}

#endif

LoadOutput::LoadOutput(PowerPort *pwr_port)
{
    port = pwr_port;
    enable = true;
    usb_enable = true;
    switch_set(false);
    usb_set(false);
    state = LOAD_STATE_DISABLED;
    usb_state = LOAD_STATE_DISABLED;
    junction_temperature = 25;              // starting point: 25°C
    overcurrent_recovery_delay = 5*60;      // default: 5 minutes
    lvd_recovery_delay = 60*60;             // default: 1 hour

    // analog comparator to detect short circuits and trigger immediate load switch-off
    short_circuit_comp_init();
}

void LoadOutput::usb_state_machine()
{
    // We don't have any overcurrent detection mechanism for USB and the buck converter IC should
    // reduce output power itself in case of failure. Battey overvoltage should also be handled by
    // the DC/DC up to a certain degree.
    // So, we don't use above failure states for the USB output. Only low SOC and exceeded
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
            if (state == LOAD_STATE_ON || state == LOAD_STATE_DISABLED) {
                // also go back to normal operation with the USB charging port
                usb_state = LOAD_STATE_DISABLED;
            }
            break;
    }
}

void LoadOutput::state_machine()
{
    switch (state) {
        case LOAD_STATE_DISABLED:
            if (enable == true) {
                if (port->pos_current_limit > 0) {
                    switch_set(true);
                    state = LOAD_STATE_ON;
                }
                else {
                    if (log_data.error_flags & (1 << ERR_BAT_UNDERVOLTAGE)) {
                        state = LOAD_STATE_OFF_LOW_SOC;
                    }
                    else {
                        // must be battery over/under temperature then
                        state = LOAD_STATE_OFF_TEMPERATURE;
                    }
                }
            }
            break;
        case LOAD_STATE_ON:
            if (enable == false) {
                switch_set(false);
                state = LOAD_STATE_DISABLED;
            }
            else if (port->pos_current_limit == 0) {  // float == is allowed for 0.0
                switch_set(false);
                // find out reason why load current is not allowed
                if (log_data.error_flags & (1 << ERR_BAT_UNDERVOLTAGE)) {
                    lvd_timestamp = time(NULL);
                    state = LOAD_STATE_OFF_LOW_SOC;
                }
                else {
                    // must be battery over/under temperature then
                    state = LOAD_STATE_OFF_TEMPERATURE;
                }
            }
            break;
        case LOAD_STATE_OFF_LOW_SOC:
            // wait at least configured time
            if (time(NULL) - lvd_timestamp > lvd_recovery_delay &&
                !(log_data.error_flags & (1 << ERR_BAT_UNDERVOLTAGE)))
            {
                state = LOAD_STATE_DISABLED; // switch to normal mode again
            }
            break;
        case LOAD_STATE_OFF_OVERCURRENT:
            // wait configured time
            if (time(NULL) - overcurrent_timestamp > overcurrent_recovery_delay) {
                log_data.error_flags &= ~(1 << ERR_LOAD_OVERCURRENT);
                log_data.error_flags &= ~(1 << ERR_LOAD_VOLTAGE_DIP);
                state = LOAD_STATE_DISABLED;   // switch to normal mode again
            }
            break;
        case LOAD_STATE_OFF_OVERVOLTAGE:
            if (port->voltage < (port->sink_voltage_max - 0.5) &&
                port->voltage < (LOW_SIDE_VOLTAGE_MAX - 0.5))
            {
                log_data.error_flags &= ~(1 << ERR_LOAD_OVERVOLTAGE);
                state = LOAD_STATE_DISABLED;   // switch to normal mode again
            }
            break;
        case LOAD_STATE_OFF_SHORT_CIRCUIT:
            // stay here until the charge controller is reset or load is switched off remotely
            if (enable == false) {
                short_circuit = false;
                log_data.error_flags &= ~(1 << ERR_LOAD_SHORT_CIRCUIT);
                state = LOAD_STATE_DISABLED;   // switch to normal mode again
            }
            break;
        case LOAD_STATE_OFF_TEMPERATURE:
            if (!(log_data.error_flags & (1U << ERR_INT_OVERTEMP)) &&
                port->pos_current_limit > 0)
            {
                state = LOAD_STATE_DISABLED;
            }
            break;
    }

    usb_state_machine();
}

// this function is called more often than the state machine
void LoadOutput::control()
{
    if (short_circuit) {
        if (state != LOAD_STATE_OFF_SHORT_CIRCUIT) {
            state = LOAD_STATE_OFF_SHORT_CIRCUIT;
            log_data.error_flags |= (1 << ERR_LOAD_SHORT_CIRCUIT);
            print_error("Load short circuit detected\n");
        }
        return;
    }

    // junction temperature calculation model for overcurrent detection
    junction_temperature = junction_temperature + (
            internal_temp - junction_temperature +
            port->current * port->current /
            (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) *
            (MOSFET_MAX_JUNCTION_TEMP - INTERNAL_MAX_REFERENCE_TEMP)
        ) / (MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY);

    if (junction_temperature > MOSFET_MAX_JUNCTION_TEMP
        || port->current > LOAD_CURRENT_MAX * 2)
    {
        switch_set(false);
        leds_flicker(LED_LOAD);
        state = LOAD_STATE_OFF_OVERCURRENT;
        log_data.error_flags |= (1 << ERR_LOAD_OVERCURRENT);
        print_error("Load overcurrent detected\n");
        overcurrent_timestamp = time(NULL);
        return;
    }

    // internal overtemperature
    if (log_data.error_flags & (1U << ERR_INT_OVERTEMP)) {
        switch_set(false);
        state = LOAD_STATE_OFF_TEMPERATURE;
        print_error("Device overtemperature detected\n");
        return;
    }

    // overcurrent detected because of voltage dip by 25% for around 100ms
    if (port->voltage < voltage_prev * 0.75 && voltage_prev != 0) {
        switch_set(false);
        leds_flicker(LED_LOAD);
        state = LOAD_STATE_OFF_OVERCURRENT;
        log_data.error_flags |= (1 << ERR_LOAD_VOLTAGE_DIP);
        print_error("Load voltage dip detected\n");
        overcurrent_timestamp = time(NULL);
        return;
    }
    voltage_prev = port->voltage;

    static int debounce_counter = 0;
    if (port->voltage > port->sink_voltage_max ||
        port->voltage > LOW_SIDE_VOLTAGE_MAX)
    {
        debounce_counter++;
        if (debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
            switch_set(false);
            print_error("Load overvoltage detected\n");
            state = LOAD_STATE_OFF_OVERVOLTAGE;
            overcurrent_timestamp = time(NULL);
            log_data.error_flags |= (1 << ERR_LOAD_OVERVOLTAGE);
            return;
        }
    }
    else {
        debounce_counter = 0;
    }
}
