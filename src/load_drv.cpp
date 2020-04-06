/*
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#ifndef UNIT_TEST

#include <inttypes.h>       // for PRIu32 in printf statements

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>

#if defined(CONFIG_SOC_SERIES_STM32L0X)
#include <stm32l0xx_ll_system.h>
#elif defined(CONFIG_SOC_SERIES_STM32G4X)
#include <stm32g4xx_ll_system.h>
#endif

static struct device *dev_load;

#ifdef DT_OUTPUTS_USB_PWR_PRESENT
static struct device *dev_usb;
#endif

#include "board.h"
#include "hardware.h"
#include "leds.h"
#include "device_status.h"
#include "debug.h"

extern LoadOutput load;     // necessary to call emergency stop function

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
    GPIOB->AFR[0] |= 0x2U << GPIO_AFRL_AFSEL2_Pos;

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

void short_circuit_comp_init()
{
#if defined(PIN_I_LOAD_COMP)

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
#if defined(__MBED__)
    NVIC_SetPriority(ADC1_COMP_IRQn, 1);
    NVIC_EnableIRQ(ADC1_COMP_IRQn);
#endif
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


void load_out_set(bool status)
{
#ifdef LED_LOAD
    leds_set(LED_LOAD, status);
#endif

#ifdef DT_OUTPUTS_LOAD_PRESENT
    gpio_pin_configure(dev_load, DT_OUTPUTS_LOAD_GPIOS_PIN,
        DT_OUTPUTS_LOAD_GPIOS_FLAGS | GPIO_OUTPUT_INACTIVE);
    if (status == true) {
#ifdef PIN_I_LOAD_COMP
        lptim_init();
#else
        gpio_pin_set(dev_load, DT_OUTPUTS_LOAD_GPIOS_PIN, 1);
#endif
    }
    else {
        gpio_pin_set(dev_load, DT_OUTPUTS_LOAD_GPIOS_PIN, 0);
    }
#endif
}

void usb_out_set(bool status)
{
#ifdef DT_OUTPUTS_USB_PWR_PRESENT
    gpio_pin_configure(dev_usb, DT_OUTPUTS_USB_PWR_GPIOS_PIN,
        DT_OUTPUTS_USB_PWR_GPIOS_FLAGS | GPIO_OUTPUT_INACTIVE);
    if (status == true) {
        gpio_pin_set(dev_usb, DT_OUTPUTS_USB_PWR_GPIOS_PIN, 1);
    }
    else {
        gpio_pin_set(dev_usb, DT_OUTPUTS_USB_PWR_GPIOS_PIN, 0);
    }
#endif
}

void load_cp_enable()
{
#ifdef DT_OUTPUTS_CHARGE_PUMP_PRESENT
	struct device *pwm_dev;
	pwm_dev = device_get_binding(DT_OUTPUTS_CHARGE_PUMP_PWMS_CONTROLLER);
	if (!pwm_dev) {
		print_error("Cannot find %s!\n", DT_OUTPUTS_CHARGE_PUMP_PWMS_CONTROLLER);
		return;
	}
    // set to 50% duty cycle
    pwm_pin_set_nsec(pwm_dev, DT_OUTPUTS_CHARGE_PUMP_PWMS_CHANNEL,
        DT_OUTPUTS_CHARGE_PUMP_PWMS_PERIOD, DT_OUTPUTS_CHARGE_PUMP_PWMS_PERIOD / 2, 0);
#endif
}

void load_out_init()
{
#ifdef DT_OUTPUTS_LOAD_PRESENT
    dev_load = device_get_binding(DT_OUTPUTS_LOAD_GPIOS_CONTROLLER);
#endif

    // analog comparator to detect short circuits and trigger immediate load switch-off
#ifdef __MBED__
    // The STM32L0 uses the same interrupt for ADC and COMP, which creates interference with the
    // Zephyr ADC driver that need still to be resolved
    short_circuit_comp_init();
#endif

    // enable charge pump for high-side switches (if existing)
    load_cp_enable();
}

void usb_out_init()
{
#ifdef DT_OUTPUTS_USB_PWR_PRESENT
    dev_usb = device_get_binding(DT_OUTPUTS_USB_PWR_GPIOS_CONTROLLER);
#endif
}

#else // UNIT_TEST

void load_out_init() {;}
void usb_out_init() {;}

void load_out_set(bool) {;}
void usb_out_set(bool) {;}

#endif
