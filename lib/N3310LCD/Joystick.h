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

#ifndef SNATCH59_JOYSTICK_H
#define SNATCH59_JOYSTICK_H

#define NUM_KEYS    5

enum eJoystickKey {LEFT_KEY, CENTER_KEY, DOWN_KEY, UP_KEY, RIGHT_KEY};

class Joystick
{
public:
    Joystick(PinName jstick);
    
    int getKeyState(int i);
    void resetKeyState(int i);
    void updateADCKey();        // call this to initiate joystick read
    
private:
    // data
    int buttonCount[NUM_KEYS];    // debounce counters
    int buttonStatus[NUM_KEYS];   // button status - pressed/released
    int buttonFlag[NUM_KEYS];     // button on flags for user program
    
    static const int adcKeyVal[NUM_KEYS];
    
    // I/O
    AnalogIn    joystick;
    
    // functions
    int getKey(int input);
};

#endif