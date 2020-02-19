# SPDX-License-Identifier: Apache-2.0

board_runner_args(pyocd "--target=stm32g431rbtx")
board_runner_args(jlink "--device=STM32G431RB" "--speed=4000" "--reset-after-load")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
