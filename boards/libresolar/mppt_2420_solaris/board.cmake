# SPDX-License-Identifier: Apache-2.0

board_runner_args(pyocd "--target=stm32g491cctx")
board_runner_args(jlink "--device=STM32G491CC" "--speed=4000" "--reset-after-load")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
