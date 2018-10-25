/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#ifdef OLED_ENABLED     // otherwise don't compile code to reduce firmware size

#include "pcb.h"
#include "data_objects.h"

#include "half_bridge.h"

#include "Adafruit_SSD1306.h"
#include "display.h"

#include "interface.h"

I2C i2c(PIN_UEXT_SDA, PIN_UEXT_SCL);
Adafruit_SSD1306_I2c oled(i2c, PIN_UEXT_SSEL, 0x78, 64, 128);

void oled_output(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load)
{
    float tmp;

    oled.clearDisplay();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (half_bridge_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load->enabled) {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }
    else {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal
    oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    oled.drawRect(66, 4, 2, 5, 1);      // bar 5

    // solar panel data
    if (half_bridge_enabled()) {
        tmp = solar_port->voltage * solar_port->current;
        oled.setTextCursor(0, 18);
        oled.printf("%4.0fW", (abs(tmp) < 1) ? 0 : tmp);     // remove negative zeros
    }
    else {
        oled.setTextCursor(8, 18);
        oled.printf("n/a");
    }
    if (solar_port->voltage > bat_port->voltage) {
        oled.setTextCursor(0, 26);
        oled.printf("%4.1fV", solar_port->voltage);
    }

    // battey data
    tmp = bat_port->voltage * (bat_port->current - load->current);
    oled.setTextCursor(42, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(42, 26);
    oled.printf("%5.1fV", bat_port->voltage);

    // load data
    tmp = bat_port->voltage * load->current;
    oled.setTextCursor(90, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fA\n", (abs(load->current) < 0.1) ? 0 : load->current);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", bat->input_Wh_day, fabs(bat->output_Wh_day));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", bat->input_Wh_total / 1000.0, fabs(bat->output_Wh_total) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("T %.0fC PWM %.0f%% SOC %d%%", dcdc->temp_mosfets, half_bridge_get_duty_cycle() * 100.0, bat->soc);

    oled.display();
}

#endif /* OLED_ENABLED */