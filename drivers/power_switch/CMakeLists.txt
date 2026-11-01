# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_include_directories(../stm32_tim)

zephyr_library_sources_ifdef(CONFIG_POWER_SWITCH_GPIO power_switch_gpio.c)
zephyr_library_sources_ifdef(CONFIG_POWER_SWITCH_STM32_LPTIM power_switch_stm32_lptim.c)
zephyr_library_sources_ifdef(CONFIG_POWER_SWITCH_STM32_PWM power_switch_stm32_pwm.c)
zephyr_library_sources_ifdef(CONFIG_POWER_SWITCH_EMUL power_switch_emul.c)
