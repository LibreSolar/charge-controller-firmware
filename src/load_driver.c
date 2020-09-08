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

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
#define LOAD_GPIO DT_CHILD(DT_PATH(outputs), load)
static const struct device *dev_load;
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
#define USB_GPIO DT_CHILD(DT_PATH(outputs), usb_pwr)
static const struct device *dev_usb;
#endif

#include "hardware.h"
#include "leds.h"
#include "mcu.h"

// short circuit detection comparator only present in PWM 2420 LUS board so far
#ifdef CONFIG_BOARD_PWM_2420_LUS

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

    // set high-speed mode (1.2us instead of 2.5us propagation delay, but 3.5uA instead of 0.5uA
    // current consumption)
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
    IRQ_CONNECT(ADC1_COMP_IRQn, 1, ADC1_COMP_IRQHandler, 0, 0);
    irq_enable(ADC1_COMP_IRQn);
}

#endif

void load_out_set(bool status)
{
#if LED_EXISTS(load)
    leds_set(LED_POS(load), status, LED_TIMEOUT_INFINITE);
#endif

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    gpio_pin_configure(dev_load, DT_GPIO_PIN(LOAD_GPIO, gpios),
        DT_GPIO_FLAGS(LOAD_GPIO, gpios) | GPIO_OUTPUT_INACTIVE);
    if (status == true) {
#ifdef CONFIG_BOARD_PWM_2420_LUS
        lptim_init();
#else
        gpio_pin_set(dev_load, DT_GPIO_PIN(LOAD_GPIO, gpios), 1);
#endif
    }
    else {
        gpio_pin_set(dev_load, DT_GPIO_PIN(LOAD_GPIO, gpios), 0);
    }
#endif
}

void usb_out_set(bool status)
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
    gpio_pin_configure(dev_usb, DT_GPIO_PIN(USB_GPIO, gpios),
        DT_GPIO_FLAGS(USB_GPIO, gpios) | GPIO_OUTPUT_INACTIVE);
    if (status == true) {
        gpio_pin_set(dev_usb, DT_GPIO_PIN(USB_GPIO, gpios), 1);
    }
    else {
        gpio_pin_set(dev_usb, DT_GPIO_PIN(USB_GPIO, gpios), 0);
    }
#endif
}

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), charge_pump))

#if defined(CONFIG_SOC_SERIES_STM32G4X)
#include <stm32g4xx_ll_tim.h>
#include <stm32g4xx_ll_rcc.h>
#include <stm32g4xx_ll_bus.h>
#endif

#define CP_PWM_PERIOD (DT_PHA(DT_CHILD(DT_PATH(outputs), charge_pump), pwms, period))
#define CP_PWM_CHANNEL (DT_PHA(DT_CHILD(DT_PATH(outputs), charge_pump), pwms, channel))

#if CP_PWM_CHANNEL == 1
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH1
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH1
#elif CP_PWM_CHANNEL == 2
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH2
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH2
#elif CP_PWM_CHANNEL == 3
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH3
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH3
#elif CP_PWM_CHANNEL == 4
#define LL_TIM_CHANNEL LL_TIM_CHANNEL_CH4
#define LL_TIM_OC_SetCompare LL_TIM_OC_SetCompareCH4
#endif

/* Currently hard-coded for TIM8 as Zephyr driver doesn't work with this timer at the moment */
void load_cp_enable()
{
    int freq_Hz = 1000*1000*1000 / CP_PWM_PERIOD;

	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM8);

    // Set timer clock to 100 kHz
    LL_TIM_SetPrescaler(TIM8, SystemCoreClock / 100000 - 1);

    LL_TIM_OC_SetMode(TIM8, LL_TIM_CHANNEL, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(TIM8, LL_TIM_CHANNEL);
    LL_TIM_OC_SetPolarity(TIM8, LL_TIM_CHANNEL, LL_TIM_OCPOLARITY_HIGH);

    // Interrupt on timer update
    LL_TIM_EnableIT_UPDATE(TIM8);

    // Force update generation (UG = 1)
    LL_TIM_GenerateEvent_UPDATE(TIM8);

    // set PWM frequency and resolution
    int _pwm_resolution = 100000 / freq_Hz;

    // Period goes from 0 to ARR (including ARR value), so substract 1 clock cycle
    LL_TIM_SetAutoReload(TIM8, _pwm_resolution - 1);

    LL_TIM_EnableCounter(TIM8);

    LL_TIM_OC_SetCompare(TIM8, _pwm_resolution / 2);    // 50% duty

    LL_TIM_CC_EnableChannel(TIM8, LL_TIM_CHANNEL);

    LL_TIM_EnableAllOutputs(TIM8);

/*
    This should do the same using the Zephyr driver, but there seems to be a bug in Zephyr v2.2
    so that it doesn't work with advanced timer as TIM8. Keep it here until Zephyr bug was fixed.

	const struct device *pwm_dev;
	pwm_dev = device_get_binding(DT_OUTPUTS_CHARGE_PUMP_PWMS_CONTROLLER);
	if (!pwm_dev) {
		LOG_ERR("Cannot find %s!\n", DT_OUTPUTS_CHARGE_PUMP_PWMS_CONTROLLER);
		return;
	}
    // set to 50% duty cycle
    pwm_pin_set_nsec(pwm_dev, CP_PWM_CHANNEL, CP_PWM_PERIOD, CP_PWM_PERIOD / 2, 0);
*/
}
#endif

void load_out_init()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
    dev_load = device_get_binding(DT_GPIO_LABEL(LOAD_GPIO, gpios));
#endif

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
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))
    dev_usb = device_get_binding(DT_GPIO_LABEL(USB_GPIO, gpios));
#endif
}

#else // UNIT_TEST

void load_out_init() {;}
void usb_out_init() {;}

void load_out_set(bool value) {;}
void usb_out_set(bool value) {;}

#endif
