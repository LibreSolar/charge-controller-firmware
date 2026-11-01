# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_DAQ daq)
add_subdirectory_ifdef(CONFIG_HALF_BRIDGE half_bridge)
add_subdirectory_ifdef(CONFIG_POWER_SWITCH power_switch)
