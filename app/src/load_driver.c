/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "load.h"

#ifdef CONFIG_SOC_FAMILY_STM32

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "board.h"
#include "hardware.h"
#include "leds.h"
#include "mcu.h"

#include <stm32_ll_bus.h>
#include <stm32_ll_system.h>

#include <stdio.h>

#if BOARD_HAS_LOAD_OUTPUT
#define LOAD_NODE DT_CHILD(DT_PATH(outputs), load)
static const struct gpio_dt_spec load_switch = GPIO_DT_SPEC_GET(LOAD_NODE, gpios);
#endif

#if BOARD_HAS_USB_OUTPUT
#define USB_NODE DT_CHILD(DT_PATH(outputs), usb_pwr)
static const struct gpio_dt_spec usb_switch = GPIO_DT_SPEC_GET(USB_NODE, gpios);
#endif

// short circuit detection comparator only present in PWM 2420 LUS board so far
#ifdef CONFIG_BOARD_PWM_2420_LUS

static void lptim_init()
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

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

    // LPTIM1->CFGR |= LPTIM_CFGR_WAVPOL;
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
    // LPTIM1->CR |= LPTIM_CR_CNTSTRT;
    // LPTIM1->CR |= LPTIM_CR_SNGSTRT;
}

void ADC1_COMP_IRQHandler(void *args)
{
    // interrupt called because of COMP2?
    if (COMP2->CSR & COMP_CSR_COMP2VALUE) {
        // Load should be switched off by LPTIM trigger already. This interrupt
        // is mainly used to indicate the failure.
        load_short_circuit_stop();
    }

    // clear interrupt flag
    EXTI->PR |= EXTI_PR_PIF22;
}

void short_circuit_comp_init()
{
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    // set GPIO pin to analog
    GPIOB->MODER &= ~(GPIO_MODER_MODE4);

    // enable VREFINT buffer
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_ENBUFLP_VREFINT_COMP;

    // select PB4 as positive input
    COMP2->CSR |= COMP_INPUT_PLUS_IO2;

    // select VREFINT divider as negative input
    COMP2->CSR |= COMP_INPUT_MINUS_1_4VREFINT;

    // propagate comparator value to LPTIM input
    COMP2->CSR |= COMP_CSR_COMP2LPTIM1IN1;

    // normal polarity
    // COMP2->CSR |= COMP_CSR_COMP2POLARITY;

    // set high-speed mode (1.2us instead of 2.5us propagation delay, but 3.5uA instead of 0.5uA
    // current consumption)
    // COMP2->CSR |= COMP_CSR_COMP2SPEED;

    // enable COMP2
    COMP2->CSR |= COMP_CSR_COMP2EN;

    // enable EXTI software interrupt / event
    EXTI->IMR |= EXTI_IMR_IM22;
    EXTI->EMR |= EXTI_EMR_EM22;
    EXTI->RTSR |= EXTI_RTSR_RT22;
    EXTI->FTSR |= EXTI_FTSR_FT22;
    EXTI->SWIER |= EXTI_SWIER_SWI22;

    // 1 = second-highest priority of STM32L0/F0
    IRQ_CONNECT(ADC1_COMP_IRQn, 1, ADC1_COMP_IRQHandler, 0, 0);
    irq_enable(ADC1_COMP_IRQn);
}

#endif

void load_out_set(bool status)
{
#if LED_EXISTS(load)
    leds_set(LED_POS(load), status, LED_TIMEOUT_INFINITE);
#endif

#if BOARD_HAS_LOAD_OUTPUT
    if (!device_is_ready(load_switch.port)) {
        printf("Load switch GPIO not ready\n");
        return;
    }
    gpio_pin_configure_dt(&load_switch, GPIO_OUTPUT_INACTIVE);
    if (status == true) {
#ifdef CONFIG_BOARD_PWM_2420_LUS
        lptim_init();
#else
        gpio_pin_set_dt(&load_switch, 1);
#endif
    }
    else {
        gpio_pin_set_dt(&load_switch, 0);
    }
#endif
}

void usb_out_set(bool status)
{
#if BOARD_HAS_USB_OUTPUT
    if (!device_is_ready(usb_switch.port)) {
        printf("USB switch GPIO not ready\n");
        return;
    }
    gpio_pin_configure_dt(&usb_switch, GPIO_OUTPUT_INACTIVE);
    if (status == true) {
        gpio_pin_set_dt(&usb_switch, 1);
#if DT_PROP(DT_CHILD(DT_PATH(outputs), usb_pwr), latching_pgood)
        k_sleep(K_MSEC(50));
        gpio_pin_configure_dt(&usb_switch, GPIO_INPUT);
#endif
    }
    else {
        gpio_pin_set_dt(&usb_switch, 0);
    }
#endif
}

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), charge_pump))

#define CP_PWMS           DT_CHILD(DT_PATH(outputs), charge_pump)
#define CP_PWM_CONTROLLER DT_PWMS_CTLR(CP_PWMS)
#define CP_PWM_PERIOD     DT_PWMS_PERIOD(CP_PWMS)
#define CP_PWM_CHANNEL    DT_PWMS_CHANNEL(CP_PWMS)

void load_cp_enable()
{
    const struct device *pwm_dev = DEVICE_DT_GET(CP_PWM_CONTROLLER);
    if (device_is_ready(pwm_dev)) {
        // set to 50% duty cycle
        pwm_set(pwm_dev, CP_PWM_CHANNEL, CP_PWM_PERIOD, CP_PWM_PERIOD / 2, 0);
    }
}

#endif

void load_out_init()
{
    // analog comparator to detect short circuits and trigger immediate load switch-off
#ifdef CONFIG_BOARD_PWM_2420_LUS
    short_circuit_comp_init();
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), charge_pump))
    // enable charge pump for high-side switches (if existing)
    load_cp_enable();
#endif
}

void usb_out_init()
{
    // nothing to do
}

bool pgood_check()
{
#if BOARD_HAS_USB_OUTPUT
    return gpio_pin_get_dt(&usb_switch);
#else
    return false;
#endif
}

#else // CONFIG_SOC_FAMILY_STM32

void load_out_init()
{}

void usb_out_init()
{}

void load_out_set(bool value)
{}

void usb_out_set(bool value)
{}

bool pgood_check()
{
    return false;
}

#endif
