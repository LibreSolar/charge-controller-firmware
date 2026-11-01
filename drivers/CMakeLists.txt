# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_DAQ daq)
add_subdirectory_ifdef(CONFIG_HALF_BRIDGE half_bridge)
add_subdirectory_ifdef(CONFIG_POWER_SWITCH power_switch)
add_subdirectory_ifdef(CONFIG_LIBRESOLAR_STM32_TIM stm32_tim)
add_subdirectory(sensor)
