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

#ifndef OLEDSSD1306_H_
#define OLEDSSD1306_H_

#if defined(__MBED__)
#include "mbed.h"
#elif defined(__ZEPHYR__)
#include <zephyr.h>
#include <drivers/i2c.h>
#endif

#include "adafruit_gfx.h"

#include <array>
#include <algorithm>

#define SSD1306_I2C_ADDRESS 0x3c    // needs to be left-shifted by 1 for mbed (=0x78)

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
#if defined(__MBED__)
    OledSSD1306(I2C &i2c, uint8_t i2cAddress = (SSD1306_I2C_ADDRESS << 1));
#elif defined(__ZEPHYR__)
    OledSSD1306(const char *i2c_name, uint8_t i2cAddress = SSD1306_I2C_ADDRESS);
#endif

    void init(uint8_t brightness = 0x01);

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void clear(void);

    void invert(bool i);

    /// Cause the display to be updated with the buffer content.
    void display();

    /// Fill the buffer with the logo splash screen.
    void splash();

    void command(uint8_t c);

    void data(uint8_t c);

protected:
    void sendBuffer();

    std::array<uint8_t, SSD1306_HEIGHT * SSD1306_WIDTH / 8> buffer;

#if defined(__MBED__)
    I2C &_i2c;
#elif defined(__ZEPHYR__)
    const struct device *_i2c;
#endif

    uint8_t _i2cAddress;
};

#endif // OLEDSSD1306_H_
