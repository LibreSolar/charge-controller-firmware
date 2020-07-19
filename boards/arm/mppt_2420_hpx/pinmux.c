/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <drivers/pinmux.h>
#include <sys/sys_io.h>

#include <pinmux/stm32/pinmux_stm32.h>

/* this should normally go to drivers/pinmux/stm32/pinmux_stm32g4x.h */
#define STM32G4X_PINMUX_FUNC_PC6_PWM8_CH1                                      \
	(STM32_PINMUX_ALT_FUNC_4 | STM32_PUSHPULL_NOPULL)

static const struct pin_config pinconf[] = {
	{STM32_PIN_PA9, STM32G4X_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32G4X_PINMUX_FUNC_PA10_USART1_RX},
	{STM32_PIN_PC6, STM32G4X_PINMUX_FUNC_PC6_PWM8_CH1},
	{STM32_PIN_PC7, STM32G4X_PINMUX_FUNC_PC7_PWM3_CH2},
#ifdef CONFIG_UART_3
	{STM32_PIN_PC10, STM32G4X_PINMUX_FUNC_PC10_USART3_TX},
	{STM32_PIN_PC11, STM32G4X_PINMUX_FUNC_PC11_USART3_RX},
#endif	/* CONFIG_UART_3 */
#ifdef CONFIG_I2C_1
	{STM32_PIN_PA15, STM32G4X_PINMUX_FUNC_PA15_I2C1_SCL},
	{STM32_PIN_PB7, STM32G4X_PINMUX_FUNC_PB7_I2C1_SDA},
#endif /* CONFIG_I2C_1 */
#ifdef CONFIG_I2C_3
	{STM32_PIN_PC8, STM32G4X_PINMUX_FUNC_PC8_I2C3_SCL},
	{STM32_PIN_PC9, STM32G4X_PINMUX_FUNC_PC9_I2C3_SDA},
#endif /* CONFIG_I2C_3 */
#ifdef CONFIG_SPI_1
	{STM32_PIN_PB3, STM32G4X_PINMUX_FUNC_PB3_SPI1_SCK},
	{STM32_PIN_PB4, STM32G4X_PINMUX_FUNC_PB4_SPI1_MISO},
	{STM32_PIN_PB5, STM32G4X_PINMUX_FUNC_PB5_SPI1_MOSI},
#endif /* CONFIG_SPI_1 */
#ifdef CONFIG_USB_DC_STM32
	{STM32_PIN_PA11, STM32G4X_PINMUX_FUNC_PA11_USB_DM},
	{STM32_PIN_PA12, STM32G4X_PINMUX_FUNC_PA12_USB_DP},
#endif	/* CONFIG_USB_DC_STM32 */
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
