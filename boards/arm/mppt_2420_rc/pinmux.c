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

static const struct pin_config pinconf[] = {
	{STM32_PIN_PA9, STM32G4X_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32G4X_PINMUX_FUNC_PA10_USART1_RX},
	{STM32_PIN_PA2, STM32G4X_PINMUX_FUNC_PA2_USART2_TX},
	{STM32_PIN_PA3, STM32G4X_PINMUX_FUNC_PA3_USART2_RX},
#ifdef CONFIG_I2C_1
	{STM32_PIN_PA15, STM32G4X_PINMUX_FUNC_PA15_I2C1_SCL},
	{STM32_PIN_PB7, STM32G4X_PINMUX_FUNC_PB7_I2C1_SDA},
#endif /* CONFIG_I2C_1 */
#ifdef CONFIG_SPI_1
	{STM32_PIN_PB3, STM32G4X_PINMUX_FUNC_PB3_SPI1_SCK},
	{STM32_PIN_PB4, STM32G4X_PINMUX_FUNC_PB4_SPI1_MISO},
	{STM32_PIN_PB5, STM32G4X_PINMUX_FUNC_PB5_SPI1_MOSI},
#endif /* CONFIG_SPI_1 */
#ifdef CONFIG_USB_DC_STM32
	{STM32_PIN_PA11, STM32G4X_PINMUX_FUNC_PA11_USB_DM},
	{STM32_PIN_PA12, STM32G4X_PINMUX_FUNC_PA12_USB_DP},
#endif	/* CONFIG_USB_DC_STM32 */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(can1), okay)
	{STM32_PIN_PA11, STM32G4X_PINMUX_FUNC_PA11_FDCAN1_RX},
	{STM32_PIN_PA12, STM32G4X_PINMUX_FUNC_PA12_FDCAN1_TX},
#endif
#if defined(CONFIG_USB_DC_STM32) && DT_NODE_HAS_STATUS(DT_NODELABEL(can1), okay)
#error "PINMUX does not allow concurrent USB and CAN."
#endif
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
	 CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);
