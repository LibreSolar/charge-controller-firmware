/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOARD_H_
#define BOARD_H_

/**
 * @file
 *
 * @brief Defines for conditional compiling based on existing board features
 */

#include <zephyr/kernel.h>

#define HALF_BRIDGE_NODE      DT_NODELABEL(halfbridge)
#define LOAD_SWITCH_NODE      DT_NODELABEL(load_switch)
#define USB_SWITCH_NODE       DT_NODELABEL(usb_switch)
#define PWM_SWITCH_NODE       DT_NODELABEL(pwm_switch)
#define NTC_FETS_NODE         DT_NODELABEL(ntc_fets)
#define NTC_BAT_NODE          DT_NODELABEL(ntc_bat)
#define BOARD_HAS_DCDC        DT_NODE_HAS_STATUS_OKAY(HALF_BRIDGE_NODE)
#define BOARD_HAS_PWM_PORT    DT_NODE_HAS_STATUS_OKAY(PWM_SWITCH_NODE)
#define BOARD_HAS_LOAD_OUTPUT DT_NODE_HAS_STATUS_OKAY(LOAD_SWITCH_NODE)
#define BOARD_HAS_USB_OUTPUT  DT_NODE_HAS_STATUS_OKAY(USB_SWITCH_NODE)
#define BOARD_HAS_TEMP_FETS   DT_NODE_HAS_STATUS_OKAY(NTC_FETS_NODE)
#define BOARD_HAS_TEMP_BAT    DT_NODE_HAS_STATUS_OKAY(NTC_BAT_NODE)

#endif /* BOARD_H_ */
