/* RIOT/cpu/stm32/include/periph_cpu.h */
/*
 * Copyright (C) 2016 Freie Universität Berlin
 *               2017 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup         cpu_stm32
 * @{
 *
 * @file
 * @brief           Shared CPU specific definitions for the STM32 family
 *
 * @author          Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author          Vincent Dupont <vincent@otakeys.com>
 */

#ifndef PERIPH_CPU_H
#define PERIPH_CPU_H

#include <soc.h>
#include <sys/util.h>

#include "periph/f3/periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Available peripheral buses
 */
typedef enum {
    APB1,           /**< APB1 bus */
    APB2,           /**< APB2 bus */
    AHB,            /**< AHB bus */
} bus_t;

#ifndef DOXYGEN
/**
 * @brief   Overwrite the default gpio_t type definition
 * @{
 */
#define HAVE_GPIO_T
typedef uint32_t gpio_t;
/** @} */
#endif

/**
 * @brief   Define a CPU specific GPIO pin generator macro
 */
#define GPIO_PIN(x, y)      ((GPIOA_BASE + (x << 10)) | y)

/**
 * @brief   Available GPIO ports
 */
enum {
#ifdef GPIOA
    PORT_A = 0,             /**< port A */
#endif
#ifdef GPIOB
    PORT_B = 1,             /**< port B */
#endif
#ifdef GPIOC
    PORT_C = 2,             /**< port C */
#endif
#ifdef GPIOD
    PORT_D = 3,             /**< port D */
#endif
#ifdef GPIOE
    PORT_E = 4,             /**< port E */
#endif
#ifdef GPIOF
    PORT_F = 5,             /**< port F */
#endif
#ifdef GPIOG
    PORT_G = 6,             /**< port G */
#endif
#ifdef GPIOH
    PORT_H = 7,             /**< port H */
#endif
#ifdef GPIOI
    PORT_I = 8,             /**< port I */
#endif
#ifdef GPIOJ
    PORT_J = 9,             /**< port J */
#endif
#ifdef GPIOK
    PORT_K = 10,            /**< port K */
#endif
};

/**
 * @brief   Available MUX values for configuring a pin's alternate function
 */
typedef enum { // gpio_af_t
    GPIO_AF0, GPIO_AF1, GPIO_AF2, GPIO_AF3, GPIO_AF4, GPIO_AF5,
    GPIO_AF6, GPIO_AF7, GPIO_AF8, GPIO_AF9, GPIO_AF10, GPIO_AF11,
    GPIO_AF12, GPIO_AF13, GPIO_AF14, GPIO_AF15
} gpio_af_t;

/**
 * @brief   ADC channel configuration data
 */
typedef struct {
    gpio_t pin;             /**< pin connected to the channel */
#if !defined(CPU_FAM_STM32F0) && !defined(CPU_FAM_STM32L0) && \
    !defined(CPU_FAM_STM32L1)
    uint8_t dev;            /**< ADCx - 1 device used for the channel */
#endif
    uint8_t chan;           /**< CPU ADC channel connected to the pin */
} adc_conf_t;

/**
 * @brief   Get the actual bus clock frequency for the APB buses
 *
 * @param[in] bus       target APBx bus
 *
 * @return              bus clock frequency in Hz
 */
uint32_t periph_apb_clk(uint8_t bus);

/**
 * @brief   Enable the given peripheral clock
 *
 * @param[in] bus       bus the peripheral is connected to
 * @param[in] mask      bit in the RCC enable register
 */
void periph_clk_en(bus_t bus, uint32_t mask);

/**
 * @brief   Configure the alternate function for the given pin
 *
 * @param[in] pin       pin to configure
 * @param[in] af        alternate function to use
 */
void gpio_init_af(gpio_t pin, gpio_af_t af);

/**
 * @brief   Configure the given pin to be used as ADC input
 *
 * @param[in] pin       pin to configure
 */
void gpio_init_analog(gpio_t pin);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
