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

#ifndef SNATCH59_N3310SPICONFIG_H
#define SNATCH59_N3310SPICONFIG_H

#include <mbed.h>

class N3310SPIPort
{
public:
    static const PinName MOSI;       // Master Out Slave In
    static const PinName MISO;       // Master In Slave Out
    static const PinName SCK;        // SPI clock
    static const PinName CE;         // Chip Enable (aka Chip Select)
    static const PinName LCD_RST;    // LCD reset
    static const PinName DAT_CMD;    // indicates if the SPI write is command or date
    static const PinName BL_ON;      // Back Light On

    static const PinName AD0;   // analog in for joystick
};

// NOTE pins have been chosen not to conflict with any I2C usage.
// MOSI = p5, MISO = p6, SCK = p7 is also an option
const PinName N3310SPIPort::MOSI = PB_5;
const PinName N3310SPIPort::MISO = NC;   // not used for 3310
const PinName N3310SPIPort::SCK = PB_3;

const PinName N3310SPIPort::CE = PA_1;
const PinName N3310SPIPort::LCD_RST = PB_6;
const PinName N3310SPIPort::DAT_CMD = PB_7;
const PinName N3310SPIPort::BL_ON = PA_2;

const PinName N3310SPIPort::AD0 = PA_3;    // joystick analog

/************************************************
*
* Nokia 3310 LCD Shield Pins
* NOTE: the LCD shield must be powered from a 3.3V supply in order
* for the joystick to be read correctly by the mbed analog in
* (which operates on a range of 0 - 3.3V).
*
* Connector J3:
* p13: SCK
* p12: MISO (not used)
* p11: MOSI
* p10: CE
* p9: LCD_RST
* p8: DAT_CMD
*
* Connector J1:
* p7: BL_ON
*
* Connector J2:
* p1 : AD0
*
**************************************************/

#endif
