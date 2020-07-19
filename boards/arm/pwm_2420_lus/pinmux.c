/*
 * Copyright (c) 2018 Ilya Tagunov <tagunil@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <drivers/pinmux.h>
#include <sys/sys_io.h>

#include <pinmux/stm32/pinmux_stm32.h>

/* this should normally go to drivers/pinmux/stm32/pinmux_stm32l0.h */
#define STM32L0_PINMUX_FUNC_PB1_PWM3_CH4                                      \
	(STM32_PINMUX_ALT_FUNC_2 | STM32_PUSHPULL_NOPULL)

static const struct pin_config pinconf[] = {
	{STM32_PIN_PB1, STM32L0_PINMUX_FUNC_PB1_PWM3_CH4},
#ifdef CONFIG_UART_1
	{STM32_PIN_PA9, STM32L0_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32L0_PINMUX_FUNC_PA10_USART1_RX},
#endif	/* CONFIG_UART_1 */
#ifdef CONFIG_UART_2
	{STM32_PIN_PA2, STM32L0_PINMUX_FUNC_PA2_USART2_TX},
	{STM32_PIN_PA3, STM32L0_PINMUX_FUNC_PA3_USART2_RX},
#endif	/* CONFIG_UART_2 */
#ifdef CONFIG_I2C_1
	{STM32_PIN_PB6, STM32L0_PINMUX_FUNC_PB6_I2C1_SCL},
	{STM32_PIN_PB7, STM32L0_PINMUX_FUNC_PB7_I2C1_SDA},
#endif /* CONFIG_I2C_1 */
#ifdef CONFIG_SPI_1
	{STM32_PIN_PB3, STM32L0_PINMUX_FUNC_PB3_SPI1_SCK},
	{STM32_PIN_PA11, STM32L0_PINMUX_FUNC_PA11_SPI1_MISO},
	{STM32_PIN_PA12, STM32L0_PINMUX_FUNC_PA12_SPI1_MOSI},
#endif /* CONFIG_SPI_1 */
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
