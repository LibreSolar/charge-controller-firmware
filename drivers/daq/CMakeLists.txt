# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(daq_common.c)
zephyr_library_sources_ifdef(CONFIG_DAQ_STM32_CHSEL daq_stm32_chsel.c)
zephyr_library_sources_ifdef(CONFIG_DAQ_STM32_RANK daq_stm32_rank.c)
zephyr_library_sources_ifdef(CONFIG_DAQ_EMUL daq_emul.c)
