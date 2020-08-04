/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <drivers/pinmux.h>
#include <sys/sys_io.h>

#include <pinmux/stm32/pinmux_stm32.h>

static const struct pin_config pinconf[] = {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(usart1), okay) && CONFIG_SERIAL
	{STM32_PIN_PA9, STM32L0_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32L0_PINMUX_FUNC_PA10_USART1_RX},
#endif /* CONFIG_SERIAL || UART_CONSOLE, usart1 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(usart2), okay) && CONFIG_SERIAL
	{STM32_PIN_PA2, STM32L0_PINMUX_FUNC_PA2_USART2_TX},
	{STM32_PIN_PA3, STM32L0_PINMUX_FUNC_PA3_USART2_RX},
#endif /* CONFIG_SERIAL, usart2 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay) && CONFIG_I2C
	{STM32_PIN_PB6, STM32L0_PINMUX_FUNC_PB6_I2C1_SCL},
	{STM32_PIN_PB7, STM32L0_PINMUX_FUNC_PB7_I2C1_SDA},
#endif /* CONFIG_I2C, i2c1 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(spi1), okay) && CONFIG_SPI
	{STM32_PIN_PB3, STM32L0_PINMUX_FUNC_PB3_SPI1_SCK},
	{STM32_PIN_PB4, STM32L0_PINMUX_FUNC_PB4_SPI1_MISO},
	{STM32_PIN_PB5, STM32L0_PINMUX_FUNC_PB5_SPI1_MOSI},
#endif /* CONFIG_SPI, spi1 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(adc1), okay) && CONFIG_ADC
	{STM32_PIN_PA0, STM32L0_PINMUX_FUNC_PA0_ADC_IN0},
#endif /* CONFIG_ADC, adc1 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(dac1), okay) && CONFIG_DAC
	{STM32_PIN_PA4, STM32L0_PINMUX_FUNC_PA4_DAC_OUT1},
#endif /* CONFIG_DAC, dac1 */
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
