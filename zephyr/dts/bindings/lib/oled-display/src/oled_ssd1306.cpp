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

#include "oled_ssd1306.h"
#include "logo.h"

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D

#if defined(__MBED__)

OledSSD1306::OledSSD1306(I2C &i2c, uint8_t i2cAddress) :
    Adafruit_GFX(SSD1306_WIDTH, SSD1306_HEIGHT),
    _i2c(i2c),
    _i2cAddress(i2cAddress)
{
    ;
}

#elif defined(__ZEPHYR__)

OledSSD1306::OledSSD1306(const char *i2c_name, uint8_t i2cAddress) :
    Adafruit_GFX(SSD1306_WIDTH, SSD1306_HEIGHT),
    _i2cAddress(i2cAddress)
{
    _i2c = device_get_binding(i2c_name);
	if (!_i2c) {
		printk("I2C: Device driver not found.\n");
	}
}

#endif

void OledSSD1306::init(uint8_t brightness)
{
    command(SSD1306_DISPLAYOFF);
    command(SSD1306_SETDISPLAYCLOCKDIV);
    command(0x80);                                  // the suggested ratio 0x80

    command(SSD1306_SETMULTIPLEX);
    command(_rawHeight-1);

    command(SSD1306_SETDISPLAYOFFSET);
    command(0x0);                                   // no offset

    command(SSD1306_SETSTARTLINE | 0x0);            // line #0

    command(SSD1306_CHARGEPUMP);
    command((SSD1306_SWITCHCAPVCC == SSD1306_EXTERNALVCC) ? 0x10 : 0x14);

    command(SSD1306_MEMORYMODE);
    command(0x00);                                  // 0x0 act like ks0108

    command(SSD1306_SEGREMAP | 0x1);

    command(SSD1306_COMSCANDEC);

    command(SSD1306_SETCOMPINS);
    command(_rawHeight == 32 ? 0x02 : 0x12);        // TODO - calculate based on _rawHieght ?

    command(SSD1306_SETCONTRAST);
    command(_rawHeight == 32 ? 0x8F : ((SSD1306_SWITCHCAPVCC == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF) );

    command(SSD1306_SETPRECHARGE);
    command((SSD1306_SWITCHCAPVCC == SSD1306_EXTERNALVCC) ? 0x22 : 0xF1);

    command(SSD1306_SETVCOMDETECT);
    command(0x40);

    command(SSD1306_DISPLAYALLON_RESUME);

    command(SSD1306_NORMALDISPLAY);

    command(SSD1306_DISPLAYON);

    // make sure that display has not shifted even without power on reset
    command(0x22);
    command(0x00);
    command(0x07);

    command(SSD1306_SETCONTRAST);
    command(brightness);

    splash();
    display();
}

void OledSSD1306::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height()))
        return;

    switch (getRotation())
    {
        case 1:
            swap(x, y);
            x = _rawWidth - x - 1;
            break;
        case 2:
            x = _rawWidth - x - 1;
            y = _rawHeight - y - 1;
            break;
        case 3:
            swap(x, y);
            y = _rawHeight - y - 1;
            break;
    }

    // x is which column
    if (color == WHITE) {
        buffer[x + (y/8)*_rawWidth] |= _BV((y % 8));
    }
    else {
        buffer[x + (y/8)*_rawWidth] &= ~_BV((y % 8));
    }
}

void OledSSD1306::invert(bool i)
{
	command(i ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

// Send the display buffer out to the display
void OledSSD1306::display(void)
{
	command(SSD1306_SETLOWCOLUMN | 0x0);    // low col = 0
	command(SSD1306_SETHIGHCOLUMN | 0x0);   // hi col = 0
	command(SSD1306_SETSTARTLINE | 0x0);    // line #0
	sendBuffer();
}

void OledSSD1306::clear(void)
{
	std::fill(buffer.begin(), buffer.end(), 0);
}

void OledSSD1306::splash(void)
{
    std::copy(&logo[0], &logo[0] + sizeof(logo), buffer.begin());
}

void OledSSD1306::command(uint8_t c)
{
    uint8_t buf[2];
    buf[0] = 0; // Command Mode
    buf[1] = c;
#if defined(__MBED__)
    _i2c.write(_i2cAddress, (char *)buf, sizeof(buf));
#elif defined(__ZEPHYR__)
    i2c_write(_i2c, buf, sizeof(buf), _i2cAddress);
#endif
}

void OledSSD1306::data(uint8_t c)
{
    uint8_t buf[2];
    buf[0] = 0x40; // Data Mode
    buf[1] = c;
#if defined(__MBED__)
    _i2c.write(_i2cAddress, (char *)buf, sizeof(buf));
#elif defined(__ZEPHYR__)
    i2c_write(_i2c, buf, sizeof(buf), _i2cAddress);
#endif
};

void OledSSD1306::sendBuffer()
{
    uint8_t buf[17];
    buf[0] = 0x40; // Data Mode

    // send display buffer in 16 byte chunks
    for (uint16_t i = 0; i < buffer.size(); i += 16)
    {
        for(uint8_t x = 1; x < sizeof(buf); x++) {
            uint16_t pos = i + x - 1;
            if (buffer.size() > pos) {
                buf[x] = buffer[pos];
            }
        }
#if defined(__MBED__)
        _i2c.write(_i2cAddress, (char *)buf, sizeof(buf));
#elif defined(__ZEPHYR__)
        i2c_write(_i2c, buf, sizeof(buf), _i2cAddress);
#endif
    }
};
