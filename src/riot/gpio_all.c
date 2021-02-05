/* RIOT/cpu/stm32/periph/gpio_all.c */
/*
 * Copyright (C) 2014-2015 Freie Universität Berlin
 *               2015 Hamburg University of Applied Sciences
 *               2017-2020 Inria
 *               2017 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cpu_stm32
 * @ingroup     drivers_periph_gpio
 * @{
 *
 * @file
 * @brief       Low-level GPIO driver implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @author      Katja Kirstein <katja.kirstein@haw-hamburg.de>
 * @author      Vincent Dupont <vincent@otakeys.com>
 *
 * @}
 */

#include "gpio.h"

/**
 * @brief   Extract the port base address from the given pin identifier
 */
static inline GPIO_TypeDef *_port(gpio_t pin)
{
    return (GPIO_TypeDef *)(pin & ~(0x0f));
}

/**
 * @brief   Extract the port number form the given identifier
 *
 * The port number is extracted by looking at bits 10, 11, 12, 13 of the base
 * register addresses.
 */
static inline int _port_num(gpio_t pin)
{
    return ((pin >> 10) & 0x0f);
}

/**
 * @brief   Extract the pin number from the last 4 bit of the pin identifier
 */
static inline int _pin_num(gpio_t pin)
{
    return (pin & 0x0f);
}

static inline void port_init_clock(GPIO_TypeDef *port, gpio_t pin)
{
    (void)port; /* <-- Only used for when port G requires special handling */
    periph_clk_en(AHB, (RCC_AHBENR_GPIOAEN << _port_num(pin)));
}

static inline void set_mode(GPIO_TypeDef *port, int pin_num, unsigned mode)
{
    uint32_t tmp = port->MODER;
    tmp &= ~(0x3 << (2 * pin_num));
    tmp |=  ((mode & 0x3) << (2 * pin_num));
    port->MODER = tmp;
}

int gpio_init(gpio_t pin, gpio_mode_t mode)
{
    GPIO_TypeDef *port = _port(pin);
    int pin_num = _pin_num(pin);

    /* enable clock */
    port_init_clock(port, pin);
    /* set mode */
    set_mode(port, pin_num, mode);
    /* set pull resistor configuration */
    port->PUPDR &= ~(0x3 << (2 * pin_num));
    port->PUPDR |=  (((mode >> 2) & 0x3) << (2 * pin_num));
    /* set output mode */
    port->OTYPER &= ~(1 << pin_num);
    port->OTYPER |=  (((mode >> 4) & 0x1) << pin_num);
    /* set pin speed to maximum */
    port->OSPEEDR |= (3 << (2 * pin_num));

    return 0;
}

void gpio_init_af(gpio_t pin, gpio_af_t af)
{
    GPIO_TypeDef *port = _port(pin);
    uint32_t pin_num = _pin_num(pin);

    /* enable clock */
    port_init_clock(port, pin);
    /* set selected function */
    port->AFR[(pin_num > 7) ? 1 : 0] &= ~(0xf << ((pin_num & 0x07) * 4));
    port->AFR[(pin_num > 7) ? 1 : 0] |= (af << ((pin_num & 0x07) * 4));
    /* set pin to AF mode */
    set_mode(port, pin_num, 2);
}

void gpio_init_analog(gpio_t pin)
{
    /* enable clock, needed as this function can be used without calling
     * gpio_init first */
    periph_clk_en(AHB, (RCC_AHBENR_GPIOAEN << _port_num(pin)));
    /* set to analog mode */
    _port(pin)->MODER |= (0x3 << (2 * _pin_num(pin)));
}
