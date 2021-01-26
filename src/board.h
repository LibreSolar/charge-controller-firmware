/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2021 Martin Jäger / Libre Solar
 */

#ifndef BOARD_H_
#define BOARD_H_

/**
 * @file
 *
 * @brief Defines for conditional compiling based on existing board features
 */

#include <zephyr.h>

#define BOARD_HAS_DCDC          DT_HAS_COMPAT_STATUS_OKAY(half_bridge)
#define BOARD_HAS_PWM_PORT      DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
#define BOARD_HAS_LOAD_OUTPUT   DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), load))
#define BOARD_HAS_USB_OUTPUT    DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), usb_pwr))

#endif /* BOARD_H_ */
