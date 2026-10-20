# Copyright (c) The Libre Solar Project Contributors
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_HALF_BRIDGE_EMUL half_bridge_emul.c)
