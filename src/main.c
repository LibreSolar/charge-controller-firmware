/*
 * Copyright (c) 2021 LAAS-CNRS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief       Test for OwnTech's high resolution timer driver integration in pwm_switch_driver.c
 *
 * @author      Hugues Larrive <hugues.larrive@laas.fr>
 */

#include <zephyr.h>
#include <stdio.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#include "pwm_switch.h"         // PWM charge controller

void main(void)
{
	printf("Libre Solar Charge Controller: %s\n", CONFIG_BOARD);

    puts("\n\
 ____________________________________________________________________________________\n\
|                                                                                    |\n\
| Test for OwnTech's high resolution timer driver integration in pwm_switch_driver.c |\n\
|____________________________________________________________________________________|\n\n");

    puts("! TIM3_IRQHandler() is not yet implemented\n");

    puts("* pwm_signal_init_registers()");
    puts("\tpwm_signal_init_registers(200000);  // 200KHz, resolution: 11520\n");
    pwm_signal_init_registers(200000);  // 200KHz, resolution: 11520

    puts("* pwm_signal_set_duty_cycle() will be called by pwm_signal_start()\n");

    puts("* pwm_signal_duty_cycle_step()");
    puts("\tpwm_signal_duty_cycle_step(5 * 11520 / 100);    // +5%\n");
    pwm_signal_duty_cycle_step(5 * 11520 / 100);    // +5%

    puts("* pwm_signal_get_duty_cycle()");
    puts("\tfloat duty_cycle = pwm_signal_get_duty_cycle();");
    float duty_cycle = pwm_signal_get_duty_cycle();
    printf("\t> duty_cycle: %f\n\n", duty_cycle);

    puts("* pwm_signal_start()");
    puts("\tpwm_signal_start(0.5);\n");
    pwm_signal_start(0.5);

    puts("\tduty_cycle = pwm_signal_get_duty_cycle();");
    duty_cycle = pwm_signal_get_duty_cycle();
    printf("\t> duty_cycle: %f\n\n", duty_cycle);

    puts("\tpwm_signal_duty_cycle_step(5 * 11520 / 100);    // +5%");
    pwm_signal_duty_cycle_step(5 * 11520 / 100);    // +5%

    puts("\tduty_cycle = pwm_signal_get_duty_cycle();");
    duty_cycle = pwm_signal_get_duty_cycle();
    printf("\t> duty_cycle: %f\n\n", duty_cycle);

    puts("*** You must observe a complementary pwm signal of 200 KHz with a duty cycle of 55% on \
PA8 and PB13");
    puts("!!! not working on PB13, PB12 is ok using TIMC OUT1.");
    puts("\tk_msleep(10000);\n");
    k_msleep(10000);

    puts("* pwm_signal_stop() will be tested after pwm_active()\n");

    puts("! pwm_signal_high() is not yet implemented\n");

    puts("* pwm_active()");
    puts("\tbool active = pwm_active();");
    bool active = pwm_active();
    printf("\t> active: %u\n\n", active);

    puts("* pwm_signal_stop()");
    puts("\tpwm_signal_stop();\n");
    pwm_signal_stop();
    
    puts("\tactive = pwm_active();");
    active = pwm_active();
    printf("\t> active: %u\n\n", active);
    
    puts("*** the end ***\n");
}
