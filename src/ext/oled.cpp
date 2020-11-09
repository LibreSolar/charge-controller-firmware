/*
 * Copyright (c) 2019 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_EXT_OLED_DISPLAY

#include <math.h>
#include <stdio.h>

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>

#include "oled_ssd1306.h"

#include "setup.h"
#include "half_bridge.h"

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), uext_en))
#define UEXT_EN_GPIO DT_CHILD(DT_PATH(outputs), uext_en)
#endif

const unsigned char bmp_load [] = {
    0x20, 0x22, 0x04, 0x70, 0x88, 0x8B, 0x88, 0x70, 0x04, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x07, 0x04, 0x07, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char bmp_arrow_right [] = {
    0x41, 0x63, 0x36, 0x1C
};

const unsigned char bmp_pv_panel [] = {
    0x60, 0x98, 0x86, 0xC9, 0x31, 0x19, 0x96, 0x62, 0x32, 0x2C, 0xC4, 0x64, 0x98, 0x08, 0xC8, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04, 0x04, 0x04, 0x03, 0x00, 0x00
};

const unsigned char bmp_disconnected [] = {
    0x08, 0x08, 0x08, 0x08, 0x00, 0x41, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x41, 0x00, 0x08, 0x08,
    0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

OledSSD1306 oled(DT_LABEL(DT_ALIAS(i2c_uext)));

void oled_update()
{
    char buf[30];
    unsigned int len;

#if CONFIG_HV_TERMINAL_SOLAR || CONFIG_PWM_TERMINAL_SOLAR
    PowerPort &in_terminal = solar_terminal;
#elif CONFIG_HV_TERMINAL_NANOGRID
    PowerPort &in_terminal = grid_terminal;
#endif

    oled.clear();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (half_bridge_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load.state == LOAD_STATE_ON) {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }
    else {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal

    if (charger.soc >= 20) {
        oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    }
    if (charger.soc >= 40) {
        oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    }
    if (charger.soc >= 60) {
        oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    }
    if (charger.soc >= 80) {
        oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    }
    if (charger.soc >= 95) {
        oled.drawRect(66, 4, 2, 5, 1);      // bar 5
    }

    // solar panel data
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    if (pwm_switch.active()) {
#else
    if (half_bridge_enabled()) {
#endif
        oled.setTextCursor(0, 18);
        len = snprintf(buf, sizeof(buf), "%4.0fW",
            (abs(in_terminal.power) < 1) ? 0 : -in_terminal.power);  // remove negative zeros
        oled.writeString(buf, len);
    }
    else {
        oled.setTextCursor(8, 18);
        len = snprintf(buf, sizeof(buf), "n/a");
        oled.writeString(buf, len);
    }
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    if (in_terminal.bus->voltage > bat_terminal.bus->voltage)
#endif
    {
        oled.setTextCursor(0, 26);
        len = snprintf(buf, sizeof(buf), "%4.1fV", in_terminal.bus->voltage);
        oled.writeString(buf, len);
    }

    // battery data
    oled.setTextCursor(42, 18);
    len = snprintf(buf, sizeof(buf), "%5.1fW",
        (abs(bat_terminal.power) < 0.1) ? 0 : bat_terminal.power);    // remove negative zeros
    oled.writeString(buf, len);
    oled.setTextCursor(42, 26);
    len = snprintf(buf, sizeof(buf), "%5.1fV", bat_terminal.bus->voltage);
    oled.writeString(buf, len);

    // load data
    oled.setTextCursor(90, 18);
    len = snprintf(buf, sizeof(buf), "%5.1fW",
        (abs(load.power) < 0.1) ? 0 : load.power);    // remove negative zeros
    oled.writeString(buf, len);
    oled.setTextCursor(90, 26);
    len = snprintf(buf, sizeof(buf), "%5.1fA\n",
        (abs(load.current) < 0.1) ? 0 : load.current);
    oled.writeString(buf, len);

    oled.setTextCursor(0, 36);
    len = snprintf(buf, sizeof(buf), "Day +%5.0fWh -%5.0fWh",
        in_terminal.neg_energy_Wh, fabs(load.pos_energy_Wh));
    oled.writeString(buf, len);
    len = snprintf(buf, sizeof(buf), "Tot +%4.1fkWh -%4.1fkWh",
        dev_stat.solar_in_total_Wh / 1000.0, fabs(dev_stat.load_out_total_Wh) / 1000.0);
    oled.writeString(buf, len);

    oled.setTextCursor(0, 56);

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), pwm_switch))
    bool pwm_enabled = pwm_switch.active();
    float duty_cycle = pwm_switch.get_duty_cycle();
#else
    bool pwm_enabled = half_bridge_enabled();
    float duty_cycle = half_bridge_get_duty_cycle();
#endif

    float temp = charger.ext_temp_sensor ? charger.bat_temperature:dev_stat.internal_temp;
    char tC = charger.ext_temp_sensor ? 'T' : 't';

    if (pwm_enabled == true) {
        len = snprintf(buf, sizeof(buf), "%c %.0fC PWM %.0f%% SOC %d%%",
            tC, temp, duty_cycle * 100.0, charger.soc);
        oled.writeString(buf, len);
    }
    else {
        len = snprintf(buf, sizeof(buf), "%c %.0fC PWM OFF SOC %d%%", tC, temp, charger.soc);
        oled.writeString(buf, len);
    }

    oled.display();
}

void oled_thread()
{
#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), uext_en))
    const struct device *dev_uext_en = device_get_binding(DT_GPIO_LABEL(UEXT_EN_GPIO, gpios));
    gpio_pin_configure(dev_uext_en, DT_GPIO_PIN(UEXT_EN_GPIO, gpios),
        DT_GPIO_FLAGS(UEXT_EN_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);
#endif

    oled.init(CONFIG_EXT_OLED_BRIGHTNESS);

    while (true) {
        oled_update();
        k_sleep(K_MSEC(1000));
    }
}

K_THREAD_DEFINE(oled_thread_id, 1024, oled_thread, NULL, NULL, NULL, 6, 0, 1000);

#endif /* CONFIG_EXT_OLED_DISPLAY */
