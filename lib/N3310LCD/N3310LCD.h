/*
* N3310LCD. A program to interface mbed with the nuelectronics
* Nokia 3310 LCD shield from www.nuelectronics.com. Ported from
* the nuelectronics Arduino code.
*
* Copyright (C) <2009> Petras Saduikis <petras@petras.co.uk>
*
* This file is part of N3310LCD.
*
* N3310LCD is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* N3310LCD is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with N3310LCD.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SNATCH59_N3310LCD_H
#define SNATCH59_N3310LCD_H

#include <mbed.h>
#include "N3310LCDDefs.h"

class N3310LCD
{
public:
    N3310LCD(PinName mosi, PinName miso, PinName sck, 
             PinName ce, PinName dat_cmd, PinName lcd_rst, PinName bl_on);
    
    void init();
    void cls();
    void backlight(eBacklight state);
    void write(BYTE data, eRequestType req_type);   
    void locate(BYTE xPos, BYTE yPos);
    
    void drawBitmap(BYTE xPos, BYTE yPos, BYTE* bitmap, BYTE bmpXSize, BYTE bmpYSize);
    void writeString(BYTE xPos, BYTE yPos, char* string, eDisplayMode mode);                  
    void writeStringBig(BYTE xPos, BYTE yPos, char* string, eDisplayMode mode);
    void writeChar(BYTE ch, eDisplayMode mode);
    void writeCharBig(BYTE xPos, BYTE yPos, BYTE ch, eDisplayMode mode);
    
private:
    // I/O
    SPI lcdPort;            // does SPI MOSI, MISO and SCK
    DigitalOut ceWire;      // does SPI CE
    DigitalOut dcWire;      // does 3310 DAT_CMD
    DigitalOut rstWire;     // does 3310 LCD_RST
    DigitalOut blWire;      // does 3310 BL_ON (backlight)    
};

#endif