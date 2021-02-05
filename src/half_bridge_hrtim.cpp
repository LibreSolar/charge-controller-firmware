/*
 * Copyright (c) 2020-2021 Centre National de la Recherche Scientifique
 *
 * CNRS, établissement public à caractère scientifique et technologique,
 * dont le siège est situé 3 rue Michel-Ange, 75794 Paris cedex 16.
 *
 *            Luiz Villa - Projet OwnTech <owntech@laas.fr>
 *        Laboratoire d'analyse et d'architecture des systèmes
 *               LAAS-CNRS - 7, avenue du Colonel Roche
 *                 BP 54200 - 31031 Toulouse cedex 4
 *
 * SPDX-License-Identifier: CECILL-C
 */

/**
 * @file
 * @brief       Low-level HRTIM driver implementation
 *
 * @author      Hugues Larrive <hugues.larrive@laas.fr>
 */

#include <zephyr.h>
#include <stdio.h>

#include "half_bridge.h"
#include "mcu.h"
#include "hrtim.h"

#if DT_NODE_EXISTS(DT_PATH(soc))
static uint16_t tim_ccr_min;        // capture/compare register min/max
static uint16_t tim_ccr_max;
static hrtim_t hrtim = 0;
static uint16_t dead_time_ns = 0;
static uint16_t hrtim_period = 0;
static uint16_t hrtim_cmp = 0;
static bool hrtim_out_enabled = 0;
#endif

static uint16_t clamp_ccr(uint16_t ccr_target)
{
    // protection against wrong settings which could destroy the hardware
    if (ccr_target <= tim_ccr_min) {
        return tim_ccr_min;
    }
    else if (ccr_target >= tim_ccr_max) {
        return tim_ccr_max;
    }
    else {
        return ccr_target;
    }
}

#define PWM_TIMER_ADDR      HRTIM1_BASE

//~ static const struct soc_gpio_pinctrl tim_pinctrl[] = ST_STM32_DT_PINCTRL(timers1, 0);

static void tim_init_registers(int freq_kHz)
{
    //~ stm32_dt_pinctrl_configure(tim_pinctrl, ARRAY_SIZE(tim_pinctrl), PWM_TIMER_ADDR);
    /**
     * HRTIM have a master timer and 5 slave timing units (tu) with two
     * outputs each. Pinout of f334r8 and f072rb are the same. PWM_HS is
     * on PA8 (TIMA OUT1) and PWM_LS is on PB13 (TIMC OUT2).
     */
    uint32_t freq = freq_kHz * 1000;
    uint16_t dead_time_ns = 0;
    hrtim_cen_t cen = (hrtim_cen_t)(TACEN | TCCEN);

    /* Initializes the master timer */
    hrtim_period = hrtim_init_master(hrtim, &freq);

    /* Initializes TIMA, set the dead time */
    hrtim_init_tu(hrtim, TIMA, &freq);
    hrtim_pwm_dt(hrtim, TIMA, dead_time_ns);

    /* Initializes TIMC, set the dead time */
    hrtim_init_tu(hrtim, TIMC, &freq);
    hrtim_pwm_dt(hrtim, TIMC, dead_time_ns);

    /* Enable counters */
    hrtim_cnt_en(hrtim, cen);

    /* setup complementary outputs on PA8 and PB13 */
    hrtim_set_cb_set(hrtim, TIMA, OUT1, MSTPER);
    hrtim_rst_cb_set(hrtim, TIMA, OUT1, MSTCMP1);
    hrtim_set_cb_set(hrtim, TIMC, OUT2, MSTCMP1);
    hrtim_rst_cb_set(hrtim, TIMC, OUT2, MSTPER);

    /* reset on master timer period event */
    hrtim_rst_evt_en(hrtim, TIMA, RST_MSTPER);
    hrtim_rst_evt_en(hrtim, TIMC, RST_MSTPER);
}

void half_bridge_start()
{
    if (hrtim_cmp >= tim_ccr_min && hrtim_cmp <= tim_ccr_max) {
        /* As it is unusual to use outputs from two timing units for a
         * single complementary output, we can not use hrtim_out_en()
         * which is able to handle two outputs but unable to handle two
         * timing units at once. */
        uint32_t oenr;

        oenr = (OUT1 << (TIMA * 2));
        oenr |= (OUT2 << (TIMC * 2));

        HRTIM1->sCommonRegs.OENR |= oenr;

        hrtim_out_enabled = 1;
    }
}

void half_bridge_stop()
{
    /* As it is unusual to use outputs from two timing units for a
     * single complementary output, we can not use hrtim_out_dis()
     * which is able to handle two outputs but unable to handle two
     * timing units at once. */
    uint32_t odisr;

    odisr = (OUT1 << (TIMA * 2));
    odisr |= (OUT2 << (TIMC * 2));

    HRTIM1->sCommonRegs.ODISR |= odisr;

    hrtim_out_enabled = 0;
}

uint16_t half_bridge_get_arr()
{
    return hrtim_period;
}

uint16_t half_bridge_get_ccr()
{
    return hrtim_cmp;
}

void half_bridge_set_ccr(uint16_t ccr)
{
    hrtim_cmp = clamp_ccr(ccr);
    hrtim_cmp_set(hrtim, MSTR, MCMP1R, hrtim_cmp);
}

bool half_bridge_enabled()
{
    return hrtim_out_enabled;
}

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    dead_time_ns = deadtime_ns;

    tim_init_registers(freq_kHz);

    tim_ccr_min = min_duty * half_bridge_get_arr();
    tim_ccr_max = max_duty * half_bridge_get_arr();

    half_bridge_set_duty_cycle(max_duty);      // init with allowed value
}

void half_bridge_set_duty_cycle(float duty)
{
    if (duty >= 0.0 && duty <= 1.0) {
        half_bridge_set_ccr(half_bridge_get_arr() * duty);
    }
}

float half_bridge_get_duty_cycle()
{
    return (float)(half_bridge_get_ccr()) / half_bridge_get_arr();
}
