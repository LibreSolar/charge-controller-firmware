/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <libresolar/drivers/half_bridge.h>

#include "oled_logo.h"
#include "setup.h"

LOG_MODULE_REGISTER(oled);

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), uext_en))
#define UEXT_EN_NODE DT_CHILD(DT_PATH(outputs), uext_en)
#endif

const unsigned char bmp_load[] = {
    0x20, 0x22, 0x04, 0x70, 0x88, 0x8B, 0x88, 0x70, 0x04, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x07, 0x04, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char bmp_arrow_right[] = { 0x41, 0x63, 0x36, 0x1C };

const unsigned char bmp_pv_panel[] = {
    0x60, 0x98, 0x86, 0xC9, 0x31, 0x19, 0x96, 0x62, 0x32, 0x2C, 0xC4, 0x64, 0x98, 0x08, 0xC8, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x04, 0x03, 0x00, 0x00,
};

const unsigned char bmp_disconnected[] = {
    0x08, 0x08, 0x08, 0x08, 0x00, 0x41, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static void draw_bitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h)
{
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (bitmap[i + (j / 8) * w] & BIT(j % 8)) {
                struct cfb_position pos = { (int16_t)(x + i), (int16_t)(y + j) };
                cfb_draw_point(display, &pos);
            }
        }
    }
}

static void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    struct cfb_position start = { x, y };
    struct cfb_position end = { (int16_t)(x + w - 1), (int16_t)(y + h - 1) };

    cfb_draw_rect(display, &start, &end);
}

void oled_update()
{
    char buf[30];

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    PowerPort &in_terminal = solar_terminal;
#elif CONFIG_HV_TERMINAL_NANOGRID
    PowerPort &in_terminal = grid_terminal;
#endif

#if BOARD_HAS_PWM_PORT
    bool pwm_enabled = pwm_switch.active();
    float duty_cycle = pwm_switch.get_duty_cycle();
#else
    bool pwm_enabled = half_bridge_is_enabled(dcdc.half_bridge);
    float duty_cycle = half_bridge_get_duty_cycle(dcdc.half_bridge);
#endif

    cfb_framebuffer_clear(display, false);

    draw_bitmap(6, 0, bmp_pv_panel, 16, 16);

    if (pwm_enabled == false) {
        draw_bitmap(27, 3, bmp_disconnected, 32, 8);
    }
    else {
        draw_bitmap(34, 3, bmp_arrow_right, 5, 7);
    }

#if BOARD_HAS_LOAD_OUTPUT
    draw_bitmap(104, 0, bmp_load, 16, 16);
    if (load.state == LOAD_STATE_ON) {
        draw_bitmap(84, 3, bmp_arrow_right, 5, 7);
    }
    else {
        draw_bitmap(81, 3, bmp_disconnected, 17, 7);
    }
#endif

    draw_rect(52, 2, 18, 9); // battery shape
    draw_rect(69, 3, 3, 7);  // battery terminal

    if (charger.soc >= 20) {
        draw_rect(54, 4, 2, 5); // bar 1
    }
    if (charger.soc >= 40) {
        draw_rect(57, 4, 2, 5); // bar 2
    }
    if (charger.soc >= 60) {
        draw_rect(60, 4, 2, 5); // bar 3
    }
    if (charger.soc >= 80) {
        draw_rect(63, 4, 2, 5); // bar 4
    }
    if (charger.soc >= 95) {
        draw_rect(66, 4, 2, 5); // bar 5
    }

    // solar panel data (values close to zero are printed as 0 to remove negative zeros)
    if (pwm_enabled) {
        snprintf(buf, sizeof(buf), "%4.0fW",
                 (fabsf(in_terminal.power) < 1.0F) ? 0.0 : -(double)in_terminal.power);
        cfb_print(display, buf, 0, 18);
    }
    else {
        cfb_print(display, "n/a", 8, 18);
    }
#if BOARD_HAS_PWM_PORT
    if (in_terminal.bus->voltage > bat_terminal.bus->voltage)
#endif
    {
        snprintf(buf, sizeof(buf), "%4.1fV", (double)in_terminal.bus->voltage);
        cfb_print(display, buf, 0, 26);
    }

    // battery data
    snprintf(buf, sizeof(buf), "%5.1fW",
             (fabsf(bat_terminal.power) < 0.1F) ? 0.0 : (double)bat_terminal.power);
    cfb_print(display, buf, 42, 18);
    snprintf(buf, sizeof(buf), "%5.1fV", (double)bat_terminal.bus->voltage);
    cfb_print(display, buf, 42, 26);

#if BOARD_HAS_LOAD_OUTPUT
    float load_energy_Wh = load.pos_energy_Wh;

    // load data
    snprintf(buf, sizeof(buf), "%5.1fW", (fabsf(load.power) < 0.1F) ? 0.0 : (double)load.power);
    cfb_print(display, buf, 90, 18);
    snprintf(buf, sizeof(buf), "%5.1fA", (fabsf(load.current) < 0.1F) ? 0.0 : (double)load.current);
    cfb_print(display, buf, 90, 26);
#else
    float load_energy_Wh = 0.0F;
#endif

    snprintf(buf, sizeof(buf), "Day +%5.0fWh -%5.0fWh", (double)in_terminal.neg_energy_Wh,
             (double)fabsf(load_energy_Wh));
    cfb_print(display, buf, 0, 36);
    snprintf(buf, sizeof(buf), "Tot +%4.1fkWh -%4.1fkWh", dev_stat.solar_in_total_Wh / 1000.0,
             fabs(dev_stat.load_out_total_Wh) / 1000.0);
    cfb_print(display, buf, 0, 44);

    float temp = charger.ext_temp_sensor ? charger.bat_temperature : dev_stat.internal_temp;
    char tC = charger.ext_temp_sensor ? 'T' : 't';

    if (pwm_enabled == true) {
        snprintf(buf, sizeof(buf), "%c %.0fC PWM %.0f%% SOC %d%%", tC, (double)temp,
                 (double)duty_cycle * 100.0, charger.soc);
    }
    else {
        snprintf(buf, sizeof(buf), "%c %.0fC PWM OFF SOC %d%%", tC, (double)temp, charger.soc);
    }
    cfb_print(display, buf, 0, 56);

    cfb_framebuffer_finalize(display);
}

static int oled_init()
{
    uint8_t font_width;
    uint8_t font_height;
    int err;

#ifdef UEXT_EN_NODE
    const struct gpio_dt_spec uext_en = GPIO_DT_SPEC_GET(UEXT_EN_NODE, gpios);

    err = gpio_pin_configure_dt(&uext_en, GPIO_OUTPUT_ACTIVE);
    if (err != 0) {
        return err;
    }

    /* the ready-time-ms of the display driver is counted from boot, not from power-on */
    k_sleep(K_MSEC(DT_PROP(DT_CHOSEN(zephyr_display), ready_time_ms)));
#endif

    /* deferred, as the UEXT port may not have been powered at boot */
    err = device_init(display);
    if (err != 0 && err != -EALREADY) {
        return err;
    }

    err = cfb_framebuffer_init(display);
    if (err != 0) {
        return err;
    }

    /* select the 5x8 font from oled_font.c with 1 pixel spacing between characters */
    for (int i = 0; i < cfb_get_numof_fonts(display); i++) {
        cfb_get_font_size(display, i, &font_width, &font_height);
        if (font_width == 5 && font_height == 8) {
            cfb_framebuffer_set_font(display, i);
            break;
        }
    }
    cfb_set_kerning(display, 1);

    draw_bitmap(0, 0, oled_logo, 128, 64);
    cfb_framebuffer_finalize(display);

    return display_blanking_off(display);
}

void oled_thread()
{
    int err = oled_init();
    if (err != 0) {
        LOG_ERR("Failed to initialize OLED display: %d", err);
        return;
    }

    /* keep the logo visible for a moment */
    k_sleep(K_SECONDS(2));

    while (true) {
        oled_update();
        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(oled_thread_id, 1024, oled_thread, NULL, NULL, NULL, 6, 0, 1000);
