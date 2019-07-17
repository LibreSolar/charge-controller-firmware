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

#include "load.h"
#include "pcb.h"
#include "config.h"
#include "hardware.h"
#include "leds.h"
#include "log.h"
#include <time.h>


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
    COMP2->CSR |= COMP_INPUT_MINUS_VREFINT;

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

volatile bool short_circuit = false;

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


void load_switch_set(bool enabled)
{
    leds_set(LED_LOAD, enabled);

#ifdef PIN_LOAD_EN
    DigitalOut load_enable(PIN_LOAD_EN);
    if (enabled) load_enable = 1;
    else load_enable = 0;
#endif

#ifdef PIN_LOAD_DIS
    DigitalOut load_disable(PIN_LOAD_DIS);
#ifdef PIN_I_LOAD_COMP
    if (enabled) lptim_init();
#else
    if (enabled) load_disable = 0;
#endif
    else load_disable = 1;
    printf("Load enabled = %d\n", enabled);
#endif
}

void load_usb_set(bool enabled)
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

void load_init(load_output_t *load, dc_bus_t *bus)
{
    load->bus = bus;
    load->current_max = LOAD_CURRENT_MAX;
    load->enabled_target = true;
    load->usb_enabled_target = true;
    load_switch_set(false);
    load_usb_set(false);
    load->switch_state = LOAD_STATE_DISABLED;
    load->usb_state = LOAD_STATE_DISABLED;
    load->junction_temperature = 25;

    // analog comparator to detect short circuits and trigger immediate load switch-off
    short_circuit_comp_init();
}

void usb_state_machine(load_output_t *load)
{
    switch (load->usb_state) {
    case LOAD_STATE_DISABLED:
        if (load->usb_enabled_target == true) {
            load_usb_set(true);
            load->usb_state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_ON:
        // currently still same cut-off SOC limit as the load
        if (load->switch_state == LOAD_STATE_OFF_LOW_SOC) {
            load_usb_set(false);
            load->usb_state = LOAD_STATE_OFF_LOW_SOC;
        }
        else if (load->switch_state == LOAD_STATE_OFF_OVERCURRENT) {
            load_usb_set(false);
            load->usb_state = LOAD_STATE_OFF_OVERCURRENT;
        }
        else if (load->usb_enabled_target == false) {
            load_usb_set(false);
            load->usb_state = LOAD_STATE_DISABLED;
        }
        break;
    case LOAD_STATE_OFF_LOW_SOC:
        // currently still same cut-off SOC limit as the load
        if (load->switch_state == LOAD_STATE_ON) {
            if (load->usb_enabled_target == true) {
                load_usb_set(true);
                load->usb_state = LOAD_STATE_ON;
            }
            else {
                load->usb_state = LOAD_STATE_DISABLED;
            }
        }
        break;
    case LOAD_STATE_OFF_OVERCURRENT:
        if (load->switch_state != LOAD_STATE_OFF_OVERCURRENT) {
            load->usb_state = LOAD_STATE_DISABLED;
        }
        break;
    }
}

static time_t lvd_timestamp = 0;    // stores when the load was disconnected last time

void load_state_machine(load_output_t *load, bool source_enabled)
{
    //printf("Load State: %d\n", load->switch_state);
    switch (load->switch_state) {
    case LOAD_STATE_DISABLED:
        if (source_enabled == true && load->enabled_target == true) {
            load_switch_set(true);
            load->switch_state = LOAD_STATE_ON;
        }
        break;
    case LOAD_STATE_ON:
        if (load->enabled_target == false) {
            load_switch_set(false);
            load->switch_state = LOAD_STATE_DISABLED;
        }
        else if (source_enabled == false) {
            lvd_timestamp = time(NULL);
            load_switch_set(false);
            load->switch_state = LOAD_STATE_OFF_LOW_SOC;
        }
        break;
    case LOAD_STATE_OFF_LOW_SOC:
        if (source_enabled == true && time(NULL) - lvd_timestamp > 60*60) {     // wait at least one hour
            if (load->enabled_target == true) {
                load_switch_set(true);
                load->switch_state = LOAD_STATE_ON;
            }
            else {
                load->switch_state = LOAD_STATE_DISABLED;
            }
        }
        break;
    case LOAD_STATE_OFF_OVERCURRENT:
        if (time(NULL) > load->overcurrent_timestamp + LOAD_OVERCURRENT_RECOVERY_TIMEOUT) {         // wait some time
            load->switch_state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    case LOAD_STATE_OFF_OVERVOLTAGE:
        if (load->bus->voltage < LOW_SIDE_VOLTAGE_MAX) {     // TODO: add hysteresis?
            load->switch_state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    case LOAD_STATE_OFF_SHORT_CIRCUIT:
        // stay here until the charge controller is reset or load is switched off remotely
        if (load->enabled_target == false) {
            short_circuit = false;
            load->switch_state = LOAD_STATE_DISABLED;   // switch to normal mode again
        }
        break;
    }

    usb_state_machine(load);
}

extern log_data_t log_data;
extern float mcu_temp;

// this function is called more often than the state machine
void load_control(load_output_t *load, float load_max_voltage)
{
    if (short_circuit) {
        load->switch_state = LOAD_STATE_OFF_SHORT_CIRCUIT;
        return;
    }

    // junction temperature calculation model for overcurrent detection
    load->junction_temperature = load->junction_temperature + (
            mcu_temp - load->junction_temperature +
            load->bus->current * load->bus->current / (LOAD_CURRENT_MAX * LOAD_CURRENT_MAX) * (MOSFET_MAX_JUNCTION_TEMP - 25)
        ) / (MOSFET_THERMAL_TIME_CONSTANT * CONTROL_FREQUENCY);

    if (load->junction_temperature > MOSFET_MAX_JUNCTION_TEMP) {
        load_switch_set(false);
        load_usb_set(false);
        load->switch_state = LOAD_STATE_OFF_OVERCURRENT;
        load->overcurrent_timestamp = time(NULL);
    }

    static int debounce_counter = 0;
    if (load->bus->voltage > load_max_voltage ||
        load->bus->voltage > LOW_SIDE_VOLTAGE_MAX)
    {
        debounce_counter++;
        if (debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
            load_switch_set(false);
            load_usb_set(false);
            load->switch_state = LOAD_STATE_OFF_OVERVOLTAGE;
            load->overcurrent_timestamp = time(NULL);
            log_data.error_flags |= (1 << ERR_BAT_OVERVOLTAGE);
        }
    }
    debounce_counter = 0;
}

#endif /* UNIT_TEST */
