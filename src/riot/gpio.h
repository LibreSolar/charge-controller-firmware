/* RIOT/drivers/include/periph/gpio.h */
/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_periph_gpio GPIO
 * @ingroup     drivers_periph
 * @brief       Low-level GPIO peripheral driver
 *
 * This is a basic GPIO (General-purpose input/output) interface to allow
 * platform independent access to a MCU's input/output pins. This interface is
 * intentionally designed to be as simple as possible, to allow for easy
 * implementation and maximum portability.
 *
 * The interface provides capabilities to initialize a pin as output-,
 * input- and interrupt pin. With the API you can basically set/clear/toggle the
 * digital signal at the hardware pin when in output mode. Configured as input you can
 * read a digital value that is being applied to the pin externally. When initializing
 * an external interrupt pin, you can register a callback function that is executed
 * in interrupt context once the interrupt condition applies to the pin. Usually you
 * can react to rising or falling signal flanks (or both).
 *
 * In addition the API provides to set standard input/output circuit modes such as
 * e.g. internal push-pull configurations.
 *
 * All modern micro controllers organize their GPIOs in some form of ports,
 * often named 'PA', 'PB', 'PC'..., or 'P0', 'P1', 'P2'..., or similar. Each of
 * these ports is then assigned a number of pins, often 8, 16, or 32. A hardware
 * pin can thus be described by its port/pin tuple. To access a pin, the
 * @p GPIO_PIN(port, pin) macro should be used. For example: If your platform has
 * a pin PB22, it will be port=1 and pin=22. The @p GPIO_PIN macro should be
 * overridden by a MCU, to allow for efficient encoding of the the port/pin tuple.
 * For example, on many platforms it is possible to `OR` the pin number with the
 * corresponding ports base register address. This allows for efficient decoding
 * of pin number and base address without the need of any address lookup.
 *
 * In case the driver does not define it, the below macro definition is used to
 * simply map the port/pin tuple to the pin value. In that case, predefined GPIO
 * definitions in `RIOT/boards/ * /include/periph_conf.h` will define the selected
 * GPIO pin.
 *
 * @warning The scalar GPIO pin type `gpio_t` is deprecated and will be
 * replaced by a structured GPIO pin type in a future GPIO API. Therefore,
 * don't use the direct comparison of GPIO pins anymore. Instead, use the
 * inline comparison functions @ref gpio_is_equal and @ref gpio_is_valid.
 *
 * # (Low-) Power Implications
 *
 * On almost all platforms, we can only control the peripheral power state of
 * full ports (i.e. groups of pins), but not for single GPIO pins. Together with
 * CPU specific alternate function handling for pins used by other peripheral
 * drivers, this can make it quite complex to keep track of pins that are
 * currently used at a certain moment. To simplify the implementations (and ease
 * the memory consumption), we expect ports to be powered on (e.g. through
 * peripheral clock gating) when first used and never be powered off again.
 *
 * GPIO driver implementations **should** power on the corresponding port during
 * gpio_init() and gpio_init_int().
 *
 * For external interrupts to work, some platforms may need to block certain
 * power modes (although this is not very likely). This should be done during
 * gpio_init_int().
 *
 * @{
 * @file
 * @brief       Low-level GPIO peripheral driver interface definitions
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef PERIPH_GPIO_H
#define PERIPH_GPIO_H

#include <limits.h>

#include "periph_cpu.h"
//~ #include "periph_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_GPIO_T
/**
 * @brief   GPIO type identifier
 */
typedef unsigned int gpio_t;
#endif

/**
 * @brief   Available pin modes
 *
 * Generally, a pin can be configured to be input or output. In output mode, a
 * pin can further be put into push-pull or open drain configuration. Though
 * this is supported by most platforms, this is not always the case, so driver
 * implementations may return an error code if a mode is not supported.
 */
#ifndef HAVE_GPIO_MODE_T
typedef enum {
    GPIO_IN ,               /**< configure as input without pull resistor */
    GPIO_IN_PD,             /**< configure as input with pull-down resistor */
    GPIO_IN_PU,             /**< configure as input with pull-up resistor */
    GPIO_OUT,               /**< configure as output in push-pull mode */
    GPIO_OD,                /**< configure as output in open-drain mode without
                             *   pull resistor */
    GPIO_OD_PU              /**< configure as output in open-drain mode with
                             *   pull resistor enabled */
} gpio_mode_t;
#endif

/**
 * @brief   Initialize the given pin as general purpose input or output
 *
 * When configured as output, the pin state after initialization is undefined.
 * The output pin's state **should** be untouched during the initialization.
 * This behavior can however **not be guaranteed** by every platform.
 *
 * @param[in] pin       pin to initialize
 * @param[in] mode      mode of the pin, see @c gpio_mode_t
 *
 * @return              0 on success
 * @return              -1 on error
 */
int gpio_init(gpio_t pin, gpio_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_GPIO_H */
/** @} */
