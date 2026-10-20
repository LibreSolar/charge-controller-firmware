# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)

# scan headers for __subsystem driver API structs (required for DEVICE_API)
zephyr_syscall_include_directories(include)

add_subdirectory(drivers)
