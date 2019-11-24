/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Adafruit Industries. All rights reserved.
 *
 * This class is partly based on the library developed by Adafruit and modified by Neal Horman
 * 7/14/2012 for use in mbed.
 *
 * Significant modifications were done by Martin Jäger (11/2019) to use it also in Zephyr
 */

#ifndef _OledSSD1306_H_
#define _OledSSD1306_H_

#include "mbed.h"
#include "Adafruit_GFX.h"

#include <array>
#include <algorithm>

#define SSD1306_I2C_ADDRESS 0x78

#define SSD1306_WIDTH	128
#define SSD1306_HEIGHT	64

#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

/** SSD1306 OLED display driver based on Adafruit GFX library
 */
class OledSSD1306 : public Adafruit_GFX
{
public:
    /** Create a SSD1306 I2C transport display driver instance
     *
     * @param i2c A reference to an initialized I2C object
     * @param i2cAddress The i2c address of the display
     * @param brightness Sets contrast between 0x01-0xFF or 1-255
     */
    OledSSD1306(I2C &i2c, uint8_t i2cAddress = SSD1306_I2C_ADDRESS, uint8_t brightness = 0x01);

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void clear(void);

    void invert(bool i);

    /// Cause the display to be updated with the buffer content.
    void display();

    /// Fill the buffer with the logo splash screen.
    void splash();

    void command(uint8_t c)
    {
        char buff[2];
        buff[0] = 0; // Command Mode
        buff[1] = c;
        mi2c.write(mi2cAddress, buff, sizeof(buff));
    }

    void data(uint8_t c)
    {
        char buff[2];
        buff[0] = 0x40; // Data Mode
        buff[1] = c;
        mi2c.write(mi2cAddress, buff, sizeof(buff));
    };

protected:
    void sendBuffer();

    std::array<uint8_t, SSD1306_HEIGHT * SSD1306_WIDTH / 8> buffer;

    I2C &mi2c;
    uint8_t mi2cAddress;
};

#endif