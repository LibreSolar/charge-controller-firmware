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

#include <mbed.h>
#include "Joystick.h"

//keypad debounce parameter
#define DEBOUNCE_MAX 15
#define DEBOUNCE_ON  10
#define DEBOUNCE_OFF 3

// values correspond to use of a 3.3V supply for the LCD shield.
const int Joystick::adcKeyVal[NUM_KEYS] = {50,     // LEFT
                                           200,    // CENTER DEPRESSED
                                           400,    // DOWN 
                                           600,    // UP
                                           800     // RIGHT
                                           // 1024 CENTER NOT DEPRESSED
                                           };
                                           
Joystick::Joystick(PinName jstick) : joystick(jstick)
{
    // reset button arrays
    for (int i = 0; i < NUM_KEYS; i++)
    {
        buttonCount[i] = 0;
        buttonStatus[i] = 0;
        buttonFlag[i] = 0;
    }
}

int Joystick::getKeyState(int i)
{
    int retval = 0;
    
    if (i < NUM_KEYS)
    {
        retval = buttonFlag[i];
    }
    
    return retval;
}

void Joystick::resetKeyState(int i)
{
    if (i < NUM_KEYS)
    {
        buttonFlag[i] = 0;
    }
}

void Joystick::updateADCKey()
{
    // NOTE: the mbed analog in is 0 - 3.3V, represented as 0.0 - 1.0. It is important 
    // that the LCD shield is powered from a 3.3V supply in order for the 'right' joystick
    // key to function correctly.
    
    int adcKeyIn = joystick * 1024;    // scale this up so we can use int
    int keyIn = getKey(adcKeyIn);
    
    for (int i = 0; i < NUM_KEYS; i++)
    {
        if (keyIn == i)  //one key is pressed 
        { 
            if (buttonCount[i] < DEBOUNCE_MAX)
            {
                buttonCount[i]++;
                if (buttonCount[i] > DEBOUNCE_ON)
                {
                    if (buttonStatus[i] == 0)
                    {
                        buttonFlag[i] = 1;
                        buttonStatus[i] = 1; //button debounced to 'pressed' status
                    }
                }
            }
        }
        else // no button pressed
        {
            if (buttonCount[i] > 0)
            {  
                buttonFlag[i] = 0;    
                buttonCount[i]--;
                if (buttonCount[i] < DEBOUNCE_OFF)
                {
                    buttonStatus[i] = 0;   //button debounced to 'released' status
                }
            }
        }
    }
}
  
// Convert ADC value to key number
int Joystick::getKey(int input)
{
    int k;
    
    for (k = 0; k < NUM_KEYS; k++)
    {
        if (input < adcKeyVal[k]) return k;
    }
    
    if (k >= NUM_KEYS) k = -1;     // No valid key pressed
    
    return k;
}
